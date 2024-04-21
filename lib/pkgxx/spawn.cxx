#include "config.h"

#include <array>
#include <cassert>
#include <cerrno>
#include <exception>
#include <fcntl.h>
#include <iterator>
#include <memory>
#if defined(HAVE_SPAWN_H)
#  include <spawn.h>
#endif
#include <string.h>
#include <system_error>
#include <unistd.h>

#include <pkgxx/fdstream.hxx>
#include <pkgxx/spawn.hxx>

#if defined(HAVE_POSIX_SPAWN) &&                             \
    defined(HAVE_POSIX_SPAWNP) &&                             \
    ( defined(HAVE_POSIX_SPAWN_FILE_ACTIONS_ADDCHDIR) ||      \
      ( defined(HAVE_POSIX_SPAWN_FILE_ACTIONS_ADDCHDIR_NP) && \
        defined(__APPLE__)                                    \
      )                                                       \
    ) &&                                                      \
    defined(HAVE_POSIX_SPAWN_FILE_ACTIONS_ADDCLOSE) &&        \
    defined(HAVE_POSIX_SPAWN_FILE_ACTIONS_ADDDUP2)
#  define USE_POSIX_SPAWN 1
#endif

#if defined(HAVE__NSGETENVIRON)
#  include <crt_externs.h>
#else
extern "C" {
    extern char** environ;
}
#endif

namespace pkgxx {
    std::array<int, 2>
    cpipe(bool set_cloexec) {
        std::array<int, 2> fds;
        if (pipe(fds.data()) != 0) {
            throw std::system_error(errno, std::generic_category(), "pipe");
        }
        if (set_cloexec) {
            if (fcntl(fds[0], F_SETFD, FD_CLOEXEC) == -1) {
                throw std::system_error(errno, std::generic_category(), "fcntl");
            }
            if (fcntl(fds[1], F_SETFD, FD_CLOEXEC) == -1) {
                throw std::system_error(errno, std::generic_category(), "fcntl");
            }
        }
        return fds;
    }

    std::map<std::string, std::string>
    cenviron() {
        std::map<std::string, std::string> env_map;
#if defined(HAVE__NSGETENVIRON)
        char** ep = *_NSGetEnviron();
#else
        char** ep = environ;
#endif
        for (; *ep; ep++) {
            std::string const es = *ep;
            auto const equal = es.find('=');
            if (equal != std::string::npos) {
                env_map.emplace(es.substr(0, equal), es.substr(equal + 1));
            }
        }
        return env_map;
    }

    namespace detail {
        /** A thin wrapper of posix_spawn_file_actions_t */
#if defined(USE_POSIX_SPAWN)
        struct file_actions {
            file_actions() {
                if (posix_spawn_file_actions_init(&_fas) != 0) {
                    throw std::system_error(
                        errno, std::generic_category(), "posix_spawn_file_actions_init");
                }
            }

            ~file_actions() noexcept(false) {
                if (posix_spawn_file_actions_destroy(&_fas) != 0) {
                    throw std::system_error(
                        errno, std::generic_category(), "posix_spawn_file_actions_destroy");
                }
            }

            operator posix_spawn_file_actions_t const* () const {
                return &_fas;
            }

            file_actions&
            chdir(std::filesystem::path const& dir) {
#  if defined(HAVE_POSIX_SPAWN_FILE_ACTIONS_ADDCHDIR)
                if (posix_spawn_file_actions_addchdir(&_fas, dir.c_str()) != 0) {
                    throw std::system_error(
                        errno, std::generic_category(), "posix_spawn_file_actions_addchdir");
                }

#  elif defined(HAVE_POSIX_SPAWN_FILE_ACTIONS_ADDCHDIR_NP) &&   \
    defined(__APPLE__)
                // Since it's a non-portable function it's not safe to use
                // without checking for platform macros. Maybe SunOS supports
                // it too, but we haven't tested it.
                if (posix_spawn_file_actions_addchdir_np(&_fas, dir.c_str()) != 0) {
                    throw std::system_error(
                        errno, std::generic_category(), "posix_spawn_file_actions_addchdir_np");
                }

#  else
#    error Bug: we have an inconsistency in the macro USE_POSIX_SPAWN
#  endif
                return *this;
            }

            file_actions&
            close_fd(int fd) {
                if (posix_spawn_file_actions_addclose(&_fas, fd) != 0) {
                    throw std::system_error(
                        errno, std::generic_category(), "posix_spawn_file_actions_addclose");
                }
                return *this;
            }

            file_actions&
            dup_fd(int from, int to) {
                if (posix_spawn_file_actions_adddup2(&_fas, from, to) != 0) {
                    throw std::system_error(
                        errno, std::generic_category(), "posix_spawn_file_actions_adddup2");
                }
                return *this;
            }

        private:
            posix_spawn_file_actions_t _fas;
        };

#else // defined(USE_POSIX_SPAWN)
        struct file_actions {
            file_actions&
            chdir(std::filesystem::path const& dir) {
                _fas.push_back(std::make_unique<fa_chdir>(dir));
                return *this;
            }

            file_actions&
            close_fd(int fd) {
                _fas.push_back(std::make_unique<fa_close>(fd));
                return *this;
            }

            file_actions&
            dup_fd(int from, int to) {
                _fas.push_back(std::make_unique<fa_dup2>(from, to));
                return *this;
            }

            void
            operator() () const {
                for (std::unique_ptr<file_action> const& fa: _fas) {
                    (*fa)();
                }
            }

        private:
            struct file_action {
                virtual ~file_action() = default;

                virtual void
                operator() () const = 0;
            };

            struct fa_chdir: file_action {
                fa_chdir(std::filesystem::path const& dir)
                    : _dir(dir) {}

                virtual void
                operator() () const override {
                    if (::chdir(_dir.c_str()) != 0) {
                        throw std::system_error(
                            errno, std::generic_category(), "chdir");
                    }
                }

            private:
                std::filesystem::path _dir;
            };

            struct fa_close: file_action {
                fa_close(int fd)
                    : _fd(fd) {}

                virtual void
                operator() () const override {
                    if (::close(_fd) != 0) {
                        throw std::system_error(
                            errno, std::generic_category(), "close");
                    }
                }

            private:
                int _fd;
            };

            struct fa_dup2: file_action {
                fa_dup2(int from, int to)
                    : _from(from)
                    , _to(to) {}

                virtual void
                operator() () const override {
                    if (::dup2(_from, _to) < 0) {
                        throw std::system_error(
                            errno, std::generic_category(), "dup2");
                    }
                }

            private:
                int _from;
                int _to;
            };

            std::vector<
                std::unique_ptr<file_action>
                > _fas;
        };
#endif // defined(USE_POSIX_SPAWN)

        spawn_base&
        spawn_base::chdir(std::filesystem::path const& dir) {
            fas().chdir(dir);
            return *this;
        }

        spawn_base&
        spawn_base::close_fd(int fd) {
            fas().close_fd(fd);
            return *this;
        }

        spawn_base&
        spawn_base::dup_fd(int from, int to) {
            fas().dup_fd(from, to);
            return *this;
        }

        pid_t
        spawn_base::operator() () const {
            std::vector<std::string> envp;
#if __cplusplus >= 202002L && __cpp_lib_optional >= 202110L
            for (auto const& [name, value]: *_env.or_else([] { return cenviron(); })) {
                envp.emplace_back(name + "=" + value);
            }
#else
            // NOTE: We can remove this cpp conditional when we switch to
            // C++23.
            if (_env) {
                for (auto const& [name, value]: *_env) {
                    envp.emplace_back(name + "=" + value);
                }
            }
            else {
                for (auto const& [name, value]: cenviron()) {
                    envp.emplace_back(name + "=" + value);
                }
            }
#endif // __cplusplus >= 202002L && __cpp_lib_optional >= 202110L

#if defined(USE_POSIX_SPAWN)
            /*
             * Nice, we can use posix_spawnp().
             */
            std::vector<char const*> cargv;
            cargv.reserve(_argv.size() + 1);
            for (auto const& arg: _argv) {
                cargv.push_back(arg.c_str());
            }
            cargv.push_back(nullptr);

            std::vector<char const*> cenvp;
            cenvp.reserve(envp.size() + 1);
            for (auto const& env: envp) {
                cenvp.push_back(env.c_str());
            }
            cenvp.push_back(nullptr);

            pid_t pid;
            if (_is_file) {
                if (posix_spawnp(
                        &pid,
                        _cmd.c_str(),
                        _fas ? *_fas : nullptr,
                        nullptr, // attrp
                        const_cast<char* const*>(cargv.data()),
                        const_cast<char* const*>(cenvp.data())) != 0) {
                    throw std::system_error(errno, std::generic_category(), "posix_spawnp");
                }
            }
            else {
                if (posix_spawn(
                        &pid,
                        _cmd.c_str(),
                        _fas ? *_fas : nullptr,
                        nullptr, // attrp
                        const_cast<char* const*>(cargv.data()),
                        const_cast<char* const*>(cenvp.data())) != 0) {
                    throw std::system_error(errno, std::generic_category(), "posix_spawnp");
                }
            }
            return pid;

#else // defined(USE_POSIX_SPAWN)
            /*
             * OMG we can't use posix_spawnp(3) OMG OMG
             */
            auto const msg_fds = cpipe(true);

#  if defined(HAVE_VFORK)
            pid_t const pid = vfork();
#  else
            pid_t const pid = fork();
#  endif
            if (pid == 0) {
                close(msg_fds[0]);
                fdostream msg_out(msg_fds[1]);

                if (_fas) {
                    try {
                        (*_fas)();
                    }
                    catch (std::system_error &e) {
                        int const code = e.code().value();
                        msg_out.write(reinterpret_cast<char const*>(&code), sizeof(int));
                        msg_out << e.what();
                        msg_out.close();
                        _exit(1);
                    }
                    catch (...) {
                        assert(0 && "must not reach here");
                        _exit(1);
                    }
                }

                std::vector<char const*> cargv;
                cargv.reserve(_argv.size() + 1);
                for (auto const& arg: _argv) {
                    cargv.push_back(arg.c_str());
                }
                cargv.push_back(nullptr);

                std::vector<char const*> cenvp;
                cenvp.reserve(envp.size() + 1);
                for (auto const& env: envp) {
                    cenvp.push_back(env.c_str());
                }
                cenvp.push_back(nullptr);

                if (_is_file) {
#  if defined(HAVE_EXECVPE)
                    if (execvpe(
                            _cmd.c_str(),
                            const_cast<char* const*>(cargv.data()),
                            const_cast<char* const*>(cenvp.data())) != 0) {
                        msg_out.write(reinterpret_cast<char const*>(&errno), sizeof(int));
                        msg_out << "execvpe";
                    }
#  else
#    if defined(HAVE__NSGETENVIRON)
                    *_NSGetEnviron() = const_cast<char **>(cenvp.data());
#    else
                    environ = const_cast<char **>(cenvp.data());
#    endif
                    if (execvp(
                            _cmd.c_str(),
                            const_cast<char* const*>(cargv.data())) != 0) {
                        msg_out.write(reinterpret_cast<char const*>(&errno), sizeof(int));
                        msg_out << "execvp";
                    }
#  endif // defined(HAVE_EXECVPE)
                }
                else {
#  if defined(HAVE_EXECVE)
                    if (execve(
                            _cmd.c_str(),
                            const_cast<char* const*>(cargv.data()),
                            const_cast<char* const*>(cenvp.data())) != 0) {
                        msg_out.write(reinterpret_cast<char const*>(&errno), sizeof(int));
                        msg_out << "execve";
                    }
#  else
#    if defined(HAVE__NSGETENVIRON)
                    *_NSGetEnviron() = const_cast<char **>(cenvp.data());
#    else
                    environ = const_cast<char **>(cenvp.data());
#    endif
                    if (execv(
                            _cmd.c_str(),
                            const_cast<char* const*>(cargv.data())) != 0) {
                        msg_out.write(reinterpret_cast<char const*>(&errno), sizeof(int));
                        msg_out << "execv";
                    }
#  endif // defined(HAVE_EXECVE)
                }

                msg_out.close();
                _exit(1);
            }
            else if (pid > 0) {
                close(msg_fds[1]);
                fdistream msg_in(msg_fds[0]);

                // The child will write errno and a string message to this
                // pipe if it fails to exec.
                int code;
                msg_in.read(reinterpret_cast<char*>(&code), sizeof(int));
                if (!msg_in.eof()) {
                    // When it successfully perform exec() the pipe will be
                    // automatically closed because we set FD_CLOEXEC on
                    // it.
                    std::string what(std::istreambuf_iterator<char>(msg_in), {});
                    throw std::system_error(
                        code, std::generic_category(), what);
                }

                return pid;
            }
            else {
                throw std::system_error(
                    errno, std::generic_category(),
#  if defined(HAVE_VFORK)
                    "vfork"
#  else
                    "fork"
#  endif
                    );
            }
#endif // defined(USE_POSIX_SPAWN)
        }

        file_actions&
        spawn_base::fas() const {
            if (!_fas) {
                _fas = std::make_shared<file_actions>();
            }
            return *_fas;
        }
    }
}
