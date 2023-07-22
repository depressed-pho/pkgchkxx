#pragma once

#include <cassert>
#include <iterator>
#include <optional>
#include <string_view>

#include <pkgxx/ordered.hxx>

namespace pkgxx {
    /** An iterator that iterates through words in a string. */
    struct word_iterator: equality_comparable<word_iterator> {
        using iterator_category = std::forward_iterator_tag; ///< The category of the iterator.
        using value_type        = std::string_view;          ///< The value of the iterator.
        using pointer           = value_type*;               ///< The pointer type of the iterator.
        using reference         = value_type&;               ///< The reference type of the iterator.

        /// Construct an invalid word iterator that acts as \c .end()
        word_iterator() {}

        /// Construct a word iterator pointing at the first word in a given
        /// string. This class acts as a reference wrapper to the string.
        /// That is, the constructor does not copy it.
        word_iterator(std::string_view const& source,
                      std::string_view const& seps   = " \t")
            : _source(source)
            , _seps(seps)
            , _pos(_source.find_first_not_of(_seps)) {

            update_current();
        }

        /// Iterator equality.
        bool
        operator== (word_iterator const& other) const noexcept {
            if (_current.has_value()) {
                return _source == other._source && other._current.has_value();
            }
            else {
                return !other._current.has_value();
            }
        }

        /// Iterator dereference.
        reference
        operator* () {
            return _current.value();
        }

        /// Iterator dereference.
        pointer
        operator-> () {
            _current.value();
            return _current.operator->();
        }

        /// Iterator incrementation.
        word_iterator&
        operator++ () {
            _pos += _current.value().size();
            _pos  = _source.find_first_not_of(_seps, _pos + 1);
            update_current();
            return *this;
        }

        /// Iterator incrementation.
        word_iterator
        operator++ (int) {
            auto it = *this;
            ++(*this);
            return it;
        }

    private:
        void
        update_current() {
            if (_pos != std::string_view::npos) {
                auto const next_sep = _source.find_first_of(_seps, _pos + 1);
                if (next_sep != std::string_view::npos) {
                    _current = _source.substr(_pos, next_sep - _pos);
                }
                else {
                    _current = _source.substr(_pos);
                }
            }
            else {
                _current.reset();
            }
        }

        std::string_view _source;
        std::string_view _seps;
        std::string_view::size_type _pos;
        std::optional<value_type> _current;
    };

    /// Split a string into words like shells do.
    struct words {
        /// The const iterator that iterates through words in a string.
        using const_iterator = word_iterator const;

        /// Construct an instance of \ref words. This class acts as a
        /// reference wrapper to the string given to the constructor. That
        /// is, the constructor does not copy the given string.
        words(std::string_view const& str,
              std::string_view const& seps = " \t")
            : _str(str)
            , _seps(seps) {}

        /// Return an iterator to the first word.
        const_iterator
        begin() const noexcept {
            return word_iterator(_str, _seps);
        }

        /// Return an iterator past the last word.
        const_iterator
        end() const noexcept {
            return word_iterator();
        }

    private:
        std::string_view const _str;
        std::string_view const _seps;
    };

    /// Remove whitespaces at the beginning and the end of a string.
    inline std::string_view
    trim(std::string_view const& str,
         std::string_view const& seps = " \t") {

        auto const begin = str.find_first_not_of(seps);
        if (begin != std::string_view::npos) {
            auto const end = str.find_last_not_of(seps);
            assert(end != std::string_view::npos);
            return str.substr(begin, end - begin + 1);
        }
        else {
            return std::string_view();
        }
    }

    /** std::string::starts_with() is a C++20 thing. We can't use it
     * atm. */
    inline bool
    starts_with(std::string_view const& str, std::string_view const& prefix) {
        return str.substr(0, prefix.size()) == prefix;
    }

    /** std::string::ends_with() is a C++20 thing. We can't use it
     * atm. */
    inline bool
    ends_with(std::string_view const& str, std::string_view const& suffix) {
        return
            str.size() >= suffix.size() &&
            str.substr(str.size() - suffix.size()) == suffix;
    }

    /// Return \c true iff the given character represents an ASCII digit.
    inline bool
    is_ascii_digit(char c) noexcept {
        return c >= '0' && c <= '9';
    }

    /// Return \c true iff the given character represents an ASCII alphabet.
    inline bool
    is_ascii_alpha(char c) noexcept {
        return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
    }

    /// Convert an ASCII upper-case letter to the corresponding lower-case
    /// one.
    inline char
    ascii_tolower(char c) noexcept {
        return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
    }

    /// Return \c true iff two strings are equivalent, ignoring case with
    /// regard to ASCII alphabets.
    inline bool
    ci_equal(std::string_view const& a, std::string_view const& b) {
        if (a.size() == b.size()) {
            for (std::string_view::size_type i = 0; i < a.size(); i++) {
                if (ascii_tolower(a[i]) != ascii_tolower(b[i])) {
                    return false;
                }
            }
            return true;
        }
        else {
            return false;
        }
    }

    /// Return \c true iff a string represented by a pair of iterators
    /// starts with a given prefix, ignoring case with regard to ASCII
    /// alphabets.
    template <typename Iter>
    bool
    ci_starts_with(Iter begin, Iter end, std::string_view const& prefix) {
        Iter it = begin;
        for (auto c: prefix) {
            if (it == end) {
                // The input is shorter than the prefix.
                return false;
            }
            else if (ascii_tolower(*it) == ascii_tolower(c)) {
                it++;
                continue;
            }
            else {
                // Letters mismatched.
                return false;
            }
        }
        return true;
    }
}
