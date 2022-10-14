#ifndef EASYSERDESJSONHELPERS_H
#define EASYSERDESJSONHELPERS_H

#include "EasySerDesCore.h"

#include <nlohmann/json.hpp>

#include <regex>

/**
 * This file contains code specific to the underlying JSON implementation
 */

namespace esd {

// FIXME remove this if it is unused
template <typename T>
[[ nodiscard ]] constexpr std::string_view TypeName()
{
    std::string_view name, prefix, suffix;
#ifdef __clang__
    name = __PRETTY_FUNCTION__;
    prefix = "std::string_view esd::TypeName() [T = ";
    suffix = "]";
#elif defined(__GNUC__)
    name = __PRETTY_FUNCTION__;
    prefix = "constexpr std::string_view esd::TypeName() [with T = ";
    suffix = "; std::string_view = std::basic_string_view<char>]";
#elif defined(_MSC_VER)
    name = __FUNCSIG__;
    prefix = "class std::basic_string_view<char,struct std::char_traits<char> > __cdecl esd::TypeName<";
    suffix = ">(void)";
#endif
    name.remove_prefix(prefix.size());
    name.remove_suffix(suffix.size());
    return name;
}

// FIXME remove this if it is unused
template<typename Arg, typename... Args>
std::string TypeNames(const std::string& seperator)
{
    std::stringstream result;
    result << TypeName<Arg>();
    if constexpr (sizeof...(Args) > 0) {
        result << seperator << TypeNames<Args...>(seperator);
    }
    return result.str();
}

// TODO the following are not JSON helpers and ought to live in core
// Useful to tell if typename T is an instance of a particular template, usage e.g. IsInstance<T, std::vector>
// FIXME needs to be tidied up!
// https://stackoverflow.com/questions/54182239/c-concepts-checking-for-template-instantiation/54191646#54191646
template <typename T, template <typename...> class Z>
struct is_specialization_of : std::false_type {};

template <typename... Args, template <typename...> class Z>
struct is_specialization_of<Z<Args...>, Z> : std::true_type {};

template<typename T, template <typename...> class Z>
concept IsSpecialisationOf = is_specialization_of<T, Z>::value;

// https://stackoverflow.com/questions/70130735/c-concept-to-check-for-derived-from-template-specialization
template <template <class...> class Template, class... Args>
void derived_from_specialization_impl(const Template<Args...>&);

template <class T, template <class...> class Template>
concept derived_from_specialization_of = requires(const T& t) {
    derived_from_specialization_impl<Template>(t);
};

template <typename T, typename BoxType>
concept TypeIsDereferencableFrom = requires (BoxType b)
{
    { *b } -> std::same_as<T&>;
};

/**
 * A helper function intended to make checking if a string can be converted to a
 * value of a particular type. Initially only supporting arithmetic types, but
 * no reason this couldn't be extended further.
 *
 * TODO these are not json helpers and ought to be in core...
 */
template <typename T>
[[ nodiscard ]] inline std::regex CreateRegex();

template <typename T>
requires std::signed_integral<T>
[[ nodiscard ]] inline std::regex CreateRegex()
{
    // Aiming for [optional + or -][sequence of 0-9, at least 1, at most std::numeric_limits<T>::digits]
    std::string regexStr = R"(^[+-]?[0-9]{1,)" + std::to_string(std::numeric_limits<T>::digits10 + 1) +  R"(}$)";
    return std::regex{ std::move(regexStr) };
}

template <typename T>
requires std::unsigned_integral<T>
[[ nodiscard ]] inline std::regex CreateRegex()
{
    // Aiming for [sequence of 0-9, at least 1, at most std::numeric_limits<T>::digits]
    std::string regexStr = R"(^[0-9]{1,)" + std::to_string(std::numeric_limits<T>::digits10 + 1) +  R"(}$)";
    return std::regex{ std::move(regexStr) };
}

template <typename T>
requires std::floating_point<T>
[[ nodiscard ]] inline std::regex CreateRegex()
{
    // FIXME So much more complex, when have brain need to limit base and mantissa digits according to std::numeric_limits
    return std::regex{ R"(^([+-]?(?:[[:d:]]+\.?|[[:d:]]*\.[[:d:]]+))(?:[Ee][+-]?[[:d:]]+)?$)" };
}

/**
 * A helper function for validating JSON content, allows parsed values of more
 * constrained numeric types to be treated as less constrained types
 */
[[ nodiscard ]] inline constexpr bool MatchType(nlohmann::json::value_t target, nlohmann::json::value_t toMatch)
{
    return target == toMatch
            || (target == nlohmann::json::value_t::number_float && (toMatch == nlohmann::json::value_t::number_integer || toMatch == nlohmann::json::value_t::number_unsigned))
            || (target == nlohmann::json::value_t::number_integer && toMatch == nlohmann::json::value_t::number_unsigned);
}

/**
 * Nlohmann JSON has a built in way to suport serialisation and deserialisation
 * of custom types, the reason for this library is to make it less work and
 * less error prone to do the same job, however it feels important to support it
 * for completeness.
 */
template <typename T>
concept SupportsNlohmannJsonSerialisation = requires (const nlohmann::json& cj, const T& ct, nlohmann::json& j, T& t) {
    { to_json(j, ct) } -> std::same_as<void>;
    { from_json(cj, t) } -> std::same_as<void>;
};

template <SupportsNlohmannJsonSerialisation T>
class JsonSerialiser<T> {
public:
    static bool Validate(const nlohmann::json&)
    {
        return true;
    }

    static nlohmann::json Serialise(const T& value)
    {
        nlohmann::json serialised;
        to_json(serialised, value);
        return serialised;
    }

    static T Deserialise(const nlohmann::json& serialised)
    {
        T value;
        from_json(serialised, value);
        return value;
    }
};

} // end namespace esd

#endif // EASYSERDESJSONHELPERS_H
