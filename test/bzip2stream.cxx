#include <array>
#include <pkgxx/bzip2stream.hxx>
#include <pkgxx/tap.hxx>
#include <sstream>

int main() {
    return pkgxx::tap{}.run([](auto& tap) {
        // This would surely compress well: 128 KiB of repeatitve data.
        std::string uncompressed;
        for (unsigned i = 0; i < 8192; i++) {
            uncompressed += "0123456789ABCDEF"; // 16 octets
        }

        std::stringstream bzip2_out;
        {
            pkgxx::bzip2ostream bzip2{bzip2_out};
            bzip2.exceptions(std::ios::badbit);
            bzip2 << uncompressed;
        }
        auto const bzipped = bzip2_out.str();
        tap.lt(
            bzipped.size(), uncompressed.size(),
            "bzipped data should be smaller than the original");

        std::stringstream bunzip2_in{bzipped};
        std::string bunzipped;
        {
            pkgxx::bunzip2istream bunzip2{bunzip2_in};
            bunzip2.exceptions(std::ios::badbit);
            std::array<char, 1024> buf;
            while (bunzip2.read(buf.data(), buf.size())) {
                bunzipped.append(
                    buf.data(),
                    static_cast<std::string::size_type>(bunzip2.gcount()));
            }
        }
        tap.is(
            bunzipped, uncompressed,
            "bzip2 => bunzip2 should roundtrip");
    });
}
