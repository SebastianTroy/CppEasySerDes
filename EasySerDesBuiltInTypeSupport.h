#ifndef EASYSERDESBUILTINTYPESUPPORT_H
#define EASYSERDESBUILTINTYPESUPPORT_H

#include "EasySerDesCore.h"
#include "EasySerDesJsonHelpers.h"

#include <nlohmann/json.hpp>

#include <concepts>
#include <sstream>
#include <regex>
#include <limits>

/**
 * This file adds support for all C++ built in types
 */

namespace util {

template <typename T>
struct JsonSerialiser;

///
/// Specialisations to support built in C++ types
///

// bool, int unsigned, float, & double could all probably be covered using an is_arithmetic concept, but then the error messages would be less helpful

template <>
struct JsonSerialiser<bool> {
    static bool Validate(const nlohmann::json& serialised)
    {
        return serialised.is_boolean();
    }

    static nlohmann::json Serialise(const bool& value)
    {
        return value;
    }

    static bool Deserialise(const nlohmann::json& serialised)
    {
        return serialised.get<bool>();
    }
};

// Specialise for char so that is is more user readable in JSON form
template <>
struct JsonSerialiser<char> {
    static bool Validate(const nlohmann::json& serialised)
    {
        return serialised.is_string() && serialised.get<std::string>().size() == 1;
    }

    static nlohmann::json Serialise(const char& value)
    {
        return std::string{ value };
    }

    static char Deserialise(const nlohmann::json& serialised)
    {
        return serialised.get<std::string>()[0];
    }
};

template <std::unsigned_integral T>
struct JsonSerialiser<T> {
    static bool Validate(const nlohmann::json& serialised)
    {
        return MatchType(nlohmann::json::value_t::number_unsigned, serialised.type()) && serialised.get<uint64_t>() == Deserialise(serialised);
    }

    static nlohmann::json Serialise(const T& value)
    {
        return value;
    }

    static T Deserialise(const nlohmann::json& serialised)
    {
        return serialised.get<T>();
    }
};

template <std::signed_integral T>
struct JsonSerialiser<T> {
    static bool Validate(const nlohmann::json& serialised)
    {
        return MatchType(nlohmann::json::value_t::number_integer, serialised.type()) && serialised.get<int64_t>() == Deserialise(serialised);
    }

    static nlohmann::json Serialise(const T& value)
    {
        return value;
    }

    static T Deserialise(const nlohmann::json& serialised)
    {
        return serialised.get<T>();
    }
};

template <std::floating_point T>
struct JsonSerialiser<T> {
    static bool Validate(const nlohmann::json& serialised)
    {
        return MatchType(nlohmann::json::value_t::number_float, serialised.type()) && serialised.get<long double>() == Deserialise(serialised);
    }

    static nlohmann::json Serialise(const T& value)
    {
        return value;
    }

    static T Deserialise(const nlohmann::json& serialised)
    {
        return serialised.get<T>();
    }
};

template <>
struct JsonSerialiser<long double> {
    static bool Validate(const nlohmann::json& serialised)
    {
        // NOT SUPPORTED NATIVELY by nlohmann_json so we need to store it in string form
        // BUT we should also support parsing smaller values that were stored as a number
        return JsonSerialiser<double>::Validate(serialised) || (serialised.is_string() && std::regex_match(serialised.get<std::string>(), rx));
    }

    static nlohmann::json Serialise(const long double& value)
    {
        // Always store the value as a string for consistency. Parse and Deserialise are more lenient to accomodate hand written/modified files
        std::stringstream valueStream;
        valueStream.precision(std::numeric_limits<long double>::digits);
        valueStream << value;
        return valueStream.str();
    }

    static long double Deserialise(const nlohmann::json& serialised)
    {
        // NOT SUPPORTED NATIVELY by nlohmann_json so we store the value as a string
        if (serialised.type() == nlohmann::json::value_t::string) {
            std::stringstream valueStream(serialised.get<std::string>());
            valueStream.precision(std::numeric_limits<long double>::digits);
            long double value;
            valueStream >> value;
            return value;
        } else {
            // Also support the parsing of smaller values that have been stored numerically
            return JsonSerialiser<double>::Deserialise(serialised);
        }
    }

private:
    static inline const std::regex rx{ R"(^([+-]?(?:[[:d:]]+\.?|[[:d:]]*\.[[:d:]]+))(?:[Ee][+-]?[[:d:]]+)?$)" };
};

template <typename T>
concept EnumConcept = std::is_enum_v<T>;
template <EnumConcept T>
struct JsonSerialiser<T> {
    static bool Validate(const nlohmann::json& serialised)
    {
        return JsonSerialiser<std::underlying_type_t<T>>::Validate(serialised);
    }

    static nlohmann::json Serialise(const T& value)
    {
        return std::bit_cast<std::underlying_type_t<T>>(value);
    }

    static T Deserialise(const nlohmann::json& serialised)
    {
        return std::bit_cast<T>(serialised.get<std::underlying_type_t<T>>());
    }
};

} // end namespace util

#endif // EASYSERDESBUILTINTYPESUPPORT_H
