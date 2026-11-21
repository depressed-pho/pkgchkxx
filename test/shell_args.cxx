#include <pkgxx/shell_args.hxx>
#include <pkgxx/tap.hxx>

int main() {
    return pkgxx::tap{}.run([](auto& tap) {
        {
            pkgxx::shell_args sa("a b c");
            tap.is(sa.size(), 3U);
            tap.is(sa.at(0), "a");
            tap.is(sa.at(1), "b");
            tap.is(sa.at(2), "c");
            tap.is(sa.string(), "a b c");
        }
        {
            pkgxx::shell_args sa("a 'b c\\' d");
            tap.is(sa.size(), 3U);
            tap.is(sa.at(0), "a");
            tap.is(sa.at(1), "b c\\");
            tap.is(sa.at(2), "d");
            tap.is(sa.string(), "a 'b c\\' d");
        }
        {
            pkgxx::shell_args sa("a \"b c\\\"\" d");
            tap.is(sa.size(), 3U);
            tap.is(sa.at(0), "a");
            tap.is(sa.at(1), "b c\"");
            tap.is(sa.at(2), "d");
            tap.is(sa.string(), "a 'b c\"' d");
        }
        {
            pkgxx::shell_args sa("a \"b\\$c'\" d");
            tap.is(sa.size(), 3U);
            tap.is(sa.at(0), "a");
            tap.is(sa.at(1), "b$c'");
            tap.is(sa.at(2), "d");
            tap.is(sa.string(), "a \"b\\$c'\" d");
        }
        {
            pkgxx::shell_args sa("a b\\ c d");
            tap.is(sa.size(), 3U);
            tap.is(sa.at(0), "a");
            tap.is(sa.at(1), "b c");
            tap.is(sa.at(2), "d");
            tap.is(sa.string(), "a 'b c' d");
        }
    });
}
