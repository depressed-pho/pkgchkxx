#pragma once

#include <filesystem>
#include <string>
#include <unordered_set>
#include <utility>

#include "value_or_ref.hxx"

namespace pkgxx {
    /** The purpose of this class is to provide a functionality similar to
     * pkgtools/varcache, but without storing the cached variables on
     * disk. It does so by querying for cacheable variables and then put
     * them in environ so that pkgsrc will not recompute them for each
     * package we scan.
     */
    struct varcache {
        varcache(std::filesystem::path const& pkgsrcdir)
            : _pkgsrcdir(pkgsrcdir)
            , _show_msg(false)
            , _populated(false) {}

        void
        default_vars() &;

        varcache&&
        default_vars() && {
            default_vars();
            return std::move(*this);
        }

        void
        add(std::string_view const& var) &;

        varcache&&
        add(std::string_view const& var) && {
            add(var);
            return std::move(*this);
        }

        void
        enable_msg(bool b) & {
            _show_msg = b;
        }

        varcache&&
        enable_msg(bool b) && {
            enable_msg(b);
            return std::move(*this);
        }

        void
        populate();

    private:
        std::filesystem::path _pkgsrcdir;
        std::unordered_set<std::string> _to_cache;
        bool _show_msg;
        bool _populated;
    };
}
