#include <algorithm>
#include <cereal/archives/portable_binary.hpp>
#include <cereal/archives/xml.hpp>
#include <cereal/version.hpp>
#include <cerrno>
#include <fstream>
#include <pkgxx/gzipstream.hxx>
#include <pkgxx/harness.hxx>
#include <pkgxx/makevars.hxx>
#include <pkgxx/mutex_guard.hxx>
#include <pkgxx/nursery.hxx>
#include <pkgxx/progress_bar.hxx>
#include <pkgxx/shell_args.hxx>
#include <pkgxx/stream.hxx>
#include <system_error>
#include <thread>
#include <unistd.h>

#include "lintdb.hxx"

namespace fs  = std::filesystem;
namespace tty = pkgxx::tty;

namespace {
    struct bad_depends_item: public std::runtime_error {
        bad_depends_item(std::string_view const& var_name,
                         std::string_view const& item)
            : std::runtime_error("")
            , var_name(var_name)
            , item(item) {}

        std::string var_name;
        std::string item;
    };
}

namespace lint {
    lintdb::lintdb(
        int,
        std::filesystem::path const& pkgsrcdir,
        std::vector<pkgxx::pkgpath> const& pkgpaths,
        pkgxx::value_or_ref<pkgxx::maybe_ttystream>&& output,
        bool list_makefiles,
        pkgxx::positive<unsigned> const& concurrency) {

        *output << tty::colour(tty::green)("Scanning Makefiles:") << std::endl;
        pkgxx::progress_bar pb{pkgpaths.size(), "output"_na = std::move(output)};
        pkgxx::guarded<decltype(_repr)> repr;
        {
            pkgxx::nursery n{concurrency};
            for (auto& pkgpath: pkgpaths) {
                n.start_soon(
                    [&]() {
                        auto const abs_pkgpath = pkgsrcdir / pkgpath;

                        if (list_makefiles) {
                            pb.message([&](auto& out) {
                                using namespace tty::literals;
                                out << tty::dull_colour(tty::magenta)(
                                           "Examining "_ch << (abs_pkgpath / "Makefile").string())
                                    << std::endl;
                            });
                        }

                        try {
                            std::vector<std::string_view> const var_names = {
                                "PKGNAME",
                                "BROKEN",
                                "BOOTSTRAP_DEPENDS",
                                "TOOL_DEPENDS",
                                "BUILD_DEPENDS",
                                "DEPENDS"
                            };
                            auto vars_map =
                                pkgxx::extract_pkgmk_vars(abs_pkgpath, var_names).value();

                            db::variables vars;
                            vars.PKGNAME = pkgxx::pkgname{vars_map["PKGNAME"]};
                            for (auto& reason: pkgxx::shell_args(vars_map["BROKEN"])) {
                                vars.BROKEN.push_back(std::move(reason));
                            }
                            auto const store_deps = [&](auto const& var_name,
                                                        std::vector<db::dependency> db::variables::* field) {
                                for (auto const& dep: pkgxx::words(vars_map[var_name])) {
                                    if (auto colon = dep.find(':'); colon != std::string_view::npos) {
                                        auto dep_pattern = dep.substr(0, colon);
                                        auto dep_path    = dep.substr(colon + 1);

                                        // Paths aren't required to start
                                        // with "../../". Canonicalise them
                                        // like what mk/scripts/pkg_path
                                        // does.
                                        auto const abs_dep_path = (abs_pkgpath / dep_path).lexically_normal();
                                        auto const dep_pkgpath  = abs_dep_path.lexically_relative(pkgsrcdir);
                                        if (dep_pkgpath.has_parent_path() && !dep_pkgpath.parent_path().has_parent_path()) {
                                            // It has exactly 2 path segments under PKGSRCDIR.
                                            try {
                                                (vars.*field).emplace_back(
                                                    pkgxx::pkgpattern{dep_pattern},
                                                    pkgxx::pkgpath{dep_pkgpath.string()});
                                            }
                                            catch (...) {
                                                std::throw_with_nested(bad_depends_item{var_name, dep});
                                            }
                                            continue;
                                        }
                                    }
                                    throw bad_depends_item{var_name, dep};
                                }
                            };
                            store_deps("BOOTSTRAP_DEPENDS", &db::variables::BOOTSTRAP_DEPENDS);
                            store_deps("TOOL_DEPENDS"     , &db::variables::TOOL_DEPENDS     );
                            store_deps("BUILD_DEPENDS"    , &db::variables::BUILD_DEPENDS    );
                            store_deps("DEPENDS"          , &db::variables::DEPENDS          );

                            pkgxx::pkgbase base = vars.PKGNAME.base; // Copy it.
                            (*repr.lock())[std::move(pkgpath)]
                                .emplace(std::move(base), std::move(vars));
                        }
                        catch (pkgxx::process_exited_for_failure const& e) {
                            // We don't appreciate it but this can
                            // happen. Some Makefiles, especially those in
                            // wip, are so broken that we can't even
                            // extract variables.
                            pb.message([&](auto& out) {
                                using namespace tty::literals;
                                out << (tty::bold + tty::colour(tty::red))(
                                           "Can't examine "_ch << (abs_pkgpath / "Makefile").string() << ": ")
                                    << tty::colour(tty::red)(e.what())
                                    << std::endl;
                            });
                        }
                        catch (pkgxx::bad_pkgname const& e) {
                            pb.message([&](auto& out) {
                                using namespace tty::literals;
                                out << (tty::bold + tty::colour(tty::red))(
                                           "Bad PKGNAME for "_ch << pkgpath << ": " << e.name)
                                    << std::endl;
                            });
                        }
                        catch (bad_depends_item const& e) {
                            pb.message([&](auto& out) {
                                using namespace tty::literals;
                                out << (tty::bold + tty::colour(tty::red))(
                                           "Bad "_ch << e.var_name << " for " << pkgpath << ": " << e.item)
                                    << std::endl;
                            });
                        }

                        // pb outlives this thread because the nursery is
                        // destructed before it.
                        pb++;
                    });
            }
        }
        _repr = repr.take();
    }

    lintdb::lintdb(std::filesystem::path const& path) {
        std::fstream file{path, std::ios_base::in};
        if (!file) {
            throw std::system_error{
                errno, std::generic_category(), "Failed to open " + path.string()};
        }

        // Peek the first 2 octets of the file. If they are 0x1F 0x8B it's
        // a gzip header and thus the file is gzipped.
        char head[2];
        file.read(head, 2);
        if (!file) {
            // Not even 2 octets could be read.
            throw std::runtime_error{"Truncated database file"};
        }
        file.putback(head[1]);
        file.putback(head[0]);

        pkgxx::any_istream raw =
            head[0] == 0x1F && head[1] == 0x8B
            ? pkgxx::any_istream{pkgxx::gunzipistream{file}}
            : pkgxx::any_istream{std::move(file)};

        // Peek the first 4 octets of the uncompressed stream.
        char magic[4];
        raw.read(magic, 4);
        if (!raw) {
            // Not even 4 octets could be read.
            throw std::runtime_error{"Truncated database file"};
        }

        if (magic[0] == 'L' && magic[1] == 'N' && magic[2] == 'D' && magic[3] == 'B') {
            // The next octet is a format type.
            auto const fmt_ch = raw.get();
            if (!raw) {
                throw std::runtime_error{"Truncated database file"};
            }

            auto const fmt = decltype(raw)::traits_type::to_char_type(fmt_ch);
            switch (fmt) {
            case 0x01:
                // It's a cereal portable binary. 3 octets follow, which
                // are version numbers of cereal.
                char vers[3];
                raw.read(vers, 3);
                if (!raw) {
                    throw std::runtime_error{"Truncated database file"};
                }
                if (vers[0] == static_cast<char>(CEREAL_VERSION_MAJOR) &&
                         vers[1] == static_cast<char>(CEREAL_VERSION_MINOR) &&
                         vers[2] == static_cast<char>(CEREAL_VERSION_PATCH)) {
                    // We know how to deserialise this!
                    cereal::PortableBinaryInputArchive ar{raw};
                    ar(cereal::make_nvp("lintdb", *this));
                }
                else {
                    throw std::runtime_error{"Database version incompatible"};
                }
                break;
            default:
                throw std::runtime_error{"Unknown database format"};
            }
        }
        else if (magic[0] == '<' && magic[1] == '?' && magic[2] == 'x' && magic[3] == 'm') {
            // This is presumably an XML file.
#if 0
            raw.putback(magic[3]);
            raw.putback(magic[2]);
            raw.putback(magic[1]);
            raw.putback(magic[0]);
            cereal::XMLInputArchive ar{raw};
            ar(cereal::make_nvp("lintdb", *this));
#else
            // For some reason it calls
            // cereal::XMLInputArchive::NodeInfo::NodeInfo() with a nullptr
            // and it dereferences the null. I haven't yet investigated the
            // cause deeply.
            throw std::runtime_error{
                "Loading a database from an XML file is temporarily disabled for now, "
                "because there seems to be a bug in cereal that leads to SIGSEGV."
            };
#endif
        }
    }

    namespace db {
        namespace detail {
            pkg_iterator::pkg_iterator(repr_type const& ref)
                : _ref{&ref}
                , _cur_bases{_ref->begin()}
                , _cur_vars{
                        *_cur_bases == _ref->end()
                        ? std::nullopt // The database is empty.
                        : std::make_optional((*_cur_bases)->second.begin())} {}

            [[gnu::pure]]
            bool
            pkg_iterator::operator== (pkg_iterator const& other) const noexcept {
                if (_cur_vars)
                    return other._cur_vars
                         ? *_cur_vars == *other._cur_vars
                         : false;
                else
                    return other._cur_vars
                         ? false
                         : true;
            }

            [[gnu::pure]]
            pkg_iterator::value_type const&
            pkg_iterator::operator* () const {
                return (*_cur_vars)->second;
            }

            pkg_iterator
            pkg_iterator::operator++ () {
                (*_cur_vars)++;
                if (*_cur_vars == (*_cur_bases)->second.end()) {
                    // Reached the end of the current inner map. Advance
                    // ourselves to the next.
                    (*_cur_bases)++;
                    if (*_cur_bases == _ref->end()) {
                        // Reached the end of everything.
                        _cur_vars.reset();
                    }
                    else {
                        _cur_vars = (*_cur_bases)->second.begin();
                    }
                }
                return *this;
            }

            pkg_iterator
            pkg_iterator::operator++ (int) {
                auto it = *this;
                ++(*this);
                return it;
            }
        }
    }

    void
    lintdb::save(std::filesystem::path const& path, lintdb::format const fmt) const {
        fs::create_directories(path.parent_path());
        std::fstream file{path, std::ios_base::out};
        if (!file) {
            throw std::system_error{
                errno, std::generic_category(), "Failed to open " + path.string()};
        }

        switch (fmt) {
        case format::bin:
            {
                pkgxx::gzipostream gz{file};
                /*
                 * Write a header in the compressed stream:
                 * +========+=========+
                 * | Magic  | Format  |
                 * +--------+---------+
                 * | 'LNDB' | 1 octet | where format 0x01 denotes cereal
                 * +========+=========+ portable binary.
                 *
                 * If the format is 0x01, 3 more octets follow:
                 * +=======+=======+=======+
                 * | Major | Minor | Patch | which are major/minor/patch
                 * +=======+=======+=======+ versions of cereal.
                 */
                gz << "LNDB"
                   << static_cast<char>(0x01)
                   << static_cast<char>(CEREAL_VERSION_MAJOR)
                   << static_cast<char>(CEREAL_VERSION_MINOR)
                   << static_cast<char>(CEREAL_VERSION_PATCH);

                cereal::PortableBinaryOutputArchive ar{gz};
                ar(cereal::make_nvp("lintdb", *this));
            }
            break;

        case format::xml:
            {
                cereal::XMLOutputArchive ar{file};
                ar(cereal::make_nvp("lintdb", *this));
            }
            break;

        default:
            std::abort(); // Impossible
        }
    }

    pkgxx::value_or_ref<pkgxx::maybe_ttystream>
    lintdb::default_output() {
        return pkgxx::value_or_ref<pkgxx::maybe_ttystream>(
            std::in_place_type<pkgxx::maybe_ttystream>, STDERR_FILENO, "owned"_na = false);
    }

    pkgxx::positive<unsigned>
    lintdb::default_concurrency() {
        return std::max(1u, std::thread::hardware_concurrency());
    }
}
