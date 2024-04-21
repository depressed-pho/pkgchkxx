/**@file        named-parameters.hpp
 * @author      Luc Hermitte <EMAIL:luc{dot}hermitte{at}gmail{dot}com>
 *
 * Distributed under the Boost Software License, Version 1.0.
 * See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt
 */
#ifndef NAMED_PARAMETERS_HPP
#define NAMED_PARAMETERS_HPP

#include <string>
#include <type_traits>
#include <utility>     //  move, forward

// # Objectives
//
// - Provide a library/framework for named-arguments in C++
// - No overhead:
//   - Everything is meant to be done at compile time
//   - No dynamic allocation is done
//   - Rely on perfect forwarding to move or reference things
// - No macros
// - Simple to use:
//   - the end-user doesn't need to define manually type for each possible
//     argument, this is done automagically thanks to C++11 user-defined literals
//   - only one function to fetch argument values, with or without default values
// - As close to C++11 as possible
//   - Unfortunatelly that wasn't possible, for simplicity reasons, this is a
//     C++14 code, that rely on a non-standard extension of gcc and clang.
// - Support
//   - references
//   - non copiable types
//   - moved stuff
//   - default values
// - Errors are detected at compilation time
//   - multiple instanciation of a same parameter
//   - missing mandatory parameters
//   - default values not compatibles with parameters
//
// # Todo:
// - permit to force the result type in get<>
// - compare assembler to the equivalent code
// - check with lambdas, static arrays, function pointers, ...
// - positional parameters for the unnamed parameters
// - add some constexpr?
// - doxygen
//
#if 0
#include <iostream>
std::string indent = "";
struct indenter {
    indenter() { indent += "  "; }
    ~indenter() { indent.resize(indent.size()-2); }
};
#endif

namespace na
{ // named arguments
    namespace literals
    {
        template <class CharT, CharT...string> struct string_literal;

        template <typename T, class CharT, CharT...string> struct proxy
        {
            using ArgumentTypeId = string_literal<CharT, string...>;
            using ArgumentType   = T;

            constexpr proxy(T&& v) : m_v(std::forward<T>(v)) {}
            static constexpr std::basic_string<CharT> name() { return {string...}; }
            constexpr T&& value() const { return std::forward<T>(m_v); }

        private:
            T && m_v;
        };

        template <class CharT, CharT...string> struct string_literal
        {
            static constexpr CharT raw_name[] = {string...};
            static constexpr std::basic_string<CharT> name() { return {string...}; }

            template <typename T>
            proxy<T, CharT, string...> operator=(T&& value) const {
                // std::cout << name() << " <- " << value << "\n";
                return proxy<T, CharT, string...>{std::forward<T>(value)};
            }
        };

        template <class CharT, CharT...string>
            constexpr string_literal<CharT, string...> operator""_na()
            { return {}; }
    } // namespace literals

    namespace internals
    {
#if 0
        /// C++17 \c void_t
        template <typename ...> using void_t = void;
#else
        /** C++17 \c void_t.
         * Implemented with a hack: Until CWG 1558 (a C++14 defect),
         * unused parameters in alias templates were not guaranteed to
         * ensure SFINAE and could be ignored, so earlier compilers
         * require a more complex definition of void_t, such as:
         * @see http://en.cppreference.com/w/cpp/types/void_t
         */
        template<typename... Ts> struct make_void { typedef void type;};
        template<typename... Ts> using void_t = typename make_void<Ts...>::type;
#endif
        /**@name boost::hana::has_common
         * @copyright Louis Dionne 2013-2016
         * Distributed under the Boost Software License, Version 1.0.
         * (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)
         * @{
         */
        template <typename T, typename U, typename = void>
            struct has_common : std::false_type  {};

        template <typename T, typename U>
            struct has_common<T, U, void_t<typename std::common_type<T, U>::type>>
            : std::true_type {};
        //@}

        struct no_default {};

        template <typename T> struct get_arg_typeid {
            using type = typename T::ArgumentTypeId;
        };
        template <typename T> struct clean_type {
            // would std::decay have been better ?
            using type = typename std::remove_cv<typename std::remove_reference<T>::type>::type;
        };

        template <typename T> struct doesnt_have_a_default
            : std::is_same<typename clean_type<T>::type, no_default>
            {};

        template <typename ArgType, typename... Args>
            struct head_is_matching : std::false_type{};
        template <typename ArgType, typename Arg0, typename... Args>
            struct head_is_matching<ArgType, Arg0, Args...>
            : std::is_same<ArgType, typename get_arg_typeid<typename clean_type<Arg0>::type>::type>
            {};

        template <typename... T> struct wrong_t {
            static constexpr bool value = false;
        };

        // Check uniqueness
        template <typename ArgType>
            inline
            void check_no_other_instanciation(ArgType&&)
            {
                // perfect: end of recursion, not found
            }

        template <typename ArgType, typename Arg0, typename... Args>
            inline
            void check_no_other_instanciation(ArgType&&, Arg0&&, Args&&... args)
            {
                // static_assert(! head_is_matching<ArgType, Arg0>::value, ArgType::raw_name);
                static_assert(! head_is_matching<ArgType, Arg0>::value, "A parameter cannot be instanciated several times");
                check_no_other_instanciation(ArgType{}, std::forward<Args>(args)...);
            }

        // Found!
        template <typename ArgType, typename Default, typename Arg0, typename... Args>
            auto && get_impl(Default&& /*default_*/, std::true_type, Arg0&& head, Args&&... tail)
            {
                static_assert(head_is_matching<ArgType, Arg0>::value, "hey!");
                static_assert(
                        doesnt_have_a_default<Default>::value ||
                        has_common<typename clean_type<Default>::type, typename clean_type<typename Arg0::ArgumentType>::type>::value,
                        "The parameter passed and the default value declared don't have compatible types!");
                // std::cout << indent << "get<"<<ArgType::name()<<">("<<Arg0::name() << ", ...) -> head:"<<"\n";
                // check there is no other instanciation of the
                // parameter
                check_no_other_instanciation(ArgType{}, std::forward<Args>(tail)...);
                return std::forward<typename Arg0::ArgumentType>(head.value());
            }

        // End of recursion: past last element
        // -> default value, optional parameter
        template <typename ArgType, typename Default>
            inline
            auto&& get_impl(Default&& default_, std::false_type)
            {
                // std::cout << indent << "get<"<<ArgType::name()<<">() -> default value: "<<default_<<"\n";
                return std::forward<Default>(default_);
            }
        // -> no default value, mandatory parameter
        enum class Invalid { Type };
        template <typename ArgType>
            inline
            auto get_impl(no_default&& default_, std::false_type)
            {
                static_assert(wrong_t<ArgType>::value, "This parameter is mandatory, no default value is acceptable");
                return Invalid::Type;
            }

        // Not found => recurse
        template <typename ArgType, typename Default, typename Arg0, typename... Args>
            inline
            auto && get_impl(Default&& default_, std::false_type, Arg0&& arg0, Args&&... tail)
            {
                static_assert(!head_is_matching<ArgType, Arg0>::value, "hey!");
                // std::cout << indent << "get<"<<ArgType::name()<<">("<<Arg0::name() << ", ...) -> recurse\n";
                using same_head = typename head_is_matching<ArgType, Args...>::type;
                // indenter id;
                return get_impl<ArgType>(std::forward<Default>(default_), same_head{}, std::forward<Args>(tail)...);
            }
    } // internals namespace

    // front-end functions
    template <typename CharT, CharT...string, typename... Args>
        inline
        auto&& get(literals::string_literal<CharT, string...> const&, Args&&... args)
        {
            using ArgType = literals::string_literal<CharT, string...>;
            using same_head = typename internals::head_is_matching<ArgType, Args...>::type;
            return internals::get_impl<ArgType>(internals::no_default{}, same_head{}, std::forward<Args>(args)...);
        }
    // Proxy => default value
    template <typename T, typename CharT, CharT...string, typename... Args>
        inline
        auto&& get(literals::proxy<T, CharT, string...> const& default_, Args&&... args)
        {
            using ArgType = literals::string_literal<CharT, string...>;
            using same_head = typename internals::head_is_matching<ArgType, Args...>::type;
            return internals::get_impl<ArgType>(default_.value(), same_head{}, std::forward<Args>(args)...);
        }

} // na namespaces

#endif // NAMED_PARAMETERS_HPP

// Vim: let $CXXFLAGS='-std=c++14 -Wno-gnu-string-literal-operator-template'
