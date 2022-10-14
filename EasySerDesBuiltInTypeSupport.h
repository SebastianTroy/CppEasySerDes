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

template <typename T>
concept IsArithmetic = std::integral<T> || std::floating_point<T>;

namespace esd {

///
/// Specialisations to support built in C++ types
///

// bool, int unsigned, float, & double could all probably be covered using an is_arithmetic concept, but then the error messages would be less helpful

template <>
class JsonSerialiser<bool> {
public:
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
class JsonSerialiser<char> {
public:
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

// Specialisation for all numeric types supported by the JSON library
template <typename T>
requires (std::signed_integral<T> && sizeof(T) <= sizeof(nlohmann::json::number_integer_t))
      || (std::unsigned_integral<T> && sizeof(T) <= sizeof(nlohmann::json::number_unsigned_t))
      || (std::floating_point<T> && sizeof(T) <= sizeof(nlohmann::json::number_float_t))
class JsonSerialiser<T> {
public:
    // TODO something like this would allow for the collapse the four Validate definitions into a single definition
    // using JsonNumericType = (std::signed_integral<T> ? nlohmann::json::number_integer_t : (std::unsigned_integral<T> ? nlohmann::json::number_unsigned_t : nlohmann::json::number_t));
    // private: static inline nlohmann::json::value_t storageType_ = ... as above ish;

    template <typename S = T>
    requires std::same_as<S, T>
          && std::signed_integral<S>
    static bool Validate(const nlohmann::json& serialised)
    {
        return Validate<nlohmann::json::number_integer_t>(serialised, nlohmann::json::value_t::number_integer);
    }

    template <typename S = T>
    requires std::same_as<S, T>
          && std::unsigned_integral<S>
    static bool Validate(const nlohmann::json& serialised)
    {
        return Validate<nlohmann::json::number_unsigned_t>(serialised, nlohmann::json::value_t::number_unsigned);
    }

    template <typename S = T>
    requires std::same_as<S, T>
          && std::floating_point<S>
    static bool Validate(const nlohmann::json& serialised)
    {
        return Validate<nlohmann::json::number_float_t>(serialised, nlohmann::json::value_t::number_float);
    }

    template <typename JsonInternalStorageType>
    static bool Validate(const nlohmann::json& serialised, const nlohmann::json::value_t& expectedStorageType)
    {
        return MatchType(serialised.type(), expectedStorageType) && serialised.get<JsonInternalStorageType>() == static_cast<JsonInternalStorageType>(Deserialise(serialised));
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

// Specialisation for all numeric types that are NOT supported by the JSON library
// FIXME only tests for "long double" as I do not have a compiler capable of larger integer types...
template <typename T>
requires (std::signed_integral<T> && sizeof(T) > sizeof(nlohmann::json::number_integer_t))
      || (std::unsigned_integral<T> && sizeof(T) > sizeof(nlohmann::json::number_unsigned_t))
      || (std::floating_point<T> && sizeof(T) > sizeof(nlohmann::json::number_float_t))
class JsonSerialiser<T> {
public:
    static bool Validate(const nlohmann::json& serialised)
    {
        return serialised.type() == nlohmann::json::value_t::string && std::regex_match(serialised.get<std::string>(), validator);
    }

    static nlohmann::json Serialise(const T& value)
    {
        std::stringstream valueStream;
        valueStream.precision(std::numeric_limits<T>::digits);
        valueStream << value;
        return valueStream.str();
    }

    static T Deserialise(const nlohmann::json& serialised)
    {
        std::stringstream valueStream(serialised.get<std::string>());
        valueStream.precision(std::numeric_limits<T>::digits);
        T value;
        valueStream >> value;
        return value;
    }

private:
    static inline std::regex validator = CreateRegex<T>();
};

template <typename T>
concept EnumConcept = std::is_enum_v<T>;

template <EnumConcept T>
class JsonSerialiser<T> {
public:
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

} // end namespace esd

#endif // EASYSERDESBUILTINTYPESUPPORT_H
