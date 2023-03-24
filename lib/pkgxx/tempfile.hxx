#pragma once

#include <filesystem>
#include <tuple>

#include <pkgxx/fdstream.hxx>

namespace pkgxx {
    /** A class holding a temporary file in an RAII way. */
    struct tempfile {
        /// Specify what to do about the created temporary file.
        enum class unlink_mode {
            /// The temporary file will be immediately unlinked from its
            /// directory. \ref tempfile::path makes no sense in this
            /// operation mode.
            immediately,
            /// The temporary file will be unlinked when the instance of
            /// \ref tempfile is destructed.
            on_destruction,
            /// The temporary file will never be unlinked automatically.
            never
        };

        /// Create a temporary file.
        tempfile(unlink_mode ul_mode_ = unlink_mode::on_destruction);
        virtual ~tempfile();

        /// The unlinking mode specified at the time when the instance is
        /// constructed.
        const unlink_mode ul_mode;

        /// The path to the created temporary file.
        std::filesystem::path const path;

        /// A read-write stream for the temporary file.
        fdstream ios;

    private:
        tempfile(
            unlink_mode ul_mode_,
            std::tuple<std::filesystem::path, fdstream>&& tmp);
    };
}
