#include <array>
#include <pkgxx/gzipstream.hxx>
#include <pkgxx/tap.hxx>
#include <sstream>

int main() {
    return pkgxx::tap{}.run([](auto& tap) {
        // This would surely compress well: 128 KiB of repeatitve data.
        std::string uncompressed;
        for (unsigned i = 0; i < 8192; i++) {
            uncompressed += "0123456789ABCDEF"; // 16 octets
        }

        std::stringstream gzip_out;
        {
            pkgxx::gzipostream gzip{gzip_out};
            gzip.exceptions(std::ios::badbit);
            gzip << uncompressed;
        }
        auto const gzipped = gzip_out.str();
        tap.lt(
            gzipped.size(), uncompressed.size(),
            "gzipped data should be smaller than the original");

        std::stringstream gunzip_in{gzipped};
        std::string gunzipped;
        {
            pkgxx::gunzipistream gunzip{gunzip_in};
            gunzip.exceptions(std::ios::badbit);
            std::array<char, 1024> buf;
            while (gunzip.read(buf.data(), buf.size())) {
                gunzipped.append(
                    buf.data(),
                    static_cast<std::string::size_type>(gunzip.gcount()));
            }
        }
        tap.is(
            gunzipped, uncompressed,
            "gzip => gunzip should roundtrip");
    });
}
