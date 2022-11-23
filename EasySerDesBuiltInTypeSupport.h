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
 *
 * A single template specialisation covering std::arithemtic could cover almost
 * everything, but then the error messages would be less helpful and the stored
 * values would be less human readable.
 *
 * Pointers and references are not supported, nor are void and nullptr_t.
 *
 * As pointers are not supported, neither are built in array types.
 *
 * Function pointers are not supported. (would this even be reasonable?)
 *
 * Member object/function pointers are not supported. (would this even be
 * reasonable?)
 */

namespace esd {

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

/**
 * Specialise for char so that is is more user readable in JSON form
 */
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

/**
 * Constrained to signed integral types supported by the JSON library
 */
template <std::signed_integral T>
requires (sizeof(T) <= sizeof(nlohmann::json::number_integer_t))
class JsonSerialiser<T> {
    public:
    static bool Validate(const nlohmann::json& serialised)
    {
        using InternalType = nlohmann::json::number_integer_t;
        return MatchType(serialised.type(), nlohmann::json::value_t::number_integer) && serialised.get<InternalType>() == static_cast<InternalType>(Deserialise(serialised));
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

/**
 * Constrained to unsigned integral types supported by the JSON library
 */
template <std::unsigned_integral T>
requires (sizeof(T) <= sizeof(nlohmann::json::number_unsigned_t))
class JsonSerialiser<T> {
    public:
    static bool Validate(const nlohmann::json& serialised)
    {
        using InternalType = nlohmann::json::number_unsigned_t;
        return MatchType(serialised.type(), nlohmann::json::value_t::number_unsigned) && serialised.get<InternalType>() == static_cast<InternalType>(Deserialise(serialised));
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

/**
 * Constrained to floating point types supported by the JSON library
 */
template <std::floating_point T>
requires (sizeof(T) <= sizeof(nlohmann::json::number_float_t))
class JsonSerialiser<T> {
public:
    static bool Validate(const nlohmann::json& serialised)
    {
        using InternalType = nlohmann::json::number_float_t;
        return MatchType(serialised.type(), nlohmann::json::value_t::number_float) && serialised.get<InternalType>() == static_cast<InternalType>(Deserialise(serialised));
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

/**
 * Specialisation for signed integer types NOT supported by the JSON library
 *
 * FIXME untested due lack of compiler support for types larger than sizeof(nlohmann::json::number_integer_t)
 */
template <std::signed_integral T>
requires (sizeof(T) > sizeof(nlohmann::json::number_integer_t))
class JsonSerialiser<T> {
    public:
    static bool Validate(const nlohmann::json& serialised)
    {
        return serialised.type() == nlohmann::json::value_t::string && std::regex_match(serialised.get<std::string>(), validator_);
    }

    static nlohmann::json Serialise(const T& value)
    {
        std::stringstream valueStream;
        valueStream.precision(std::numeric_limits<T>::max_digits10);
        valueStream << value;
        return valueStream.str();
    }

    static T Deserialise(const nlohmann::json& serialised)
    {
        std::stringstream valueStream(serialised.get<std::string>());
        valueStream.precision(std::numeric_limits<T>::max_digits10);
        T value;
        valueStream >> value;
        return value;
    }

private:
    // Aiming for [optional + or -][sequence of 0-9, at least 1, at most std::numeric_limits<T>::max_digits10]
    // FIXME does not constrain values to between std::numeric_limits<T>::min and std::numeric_limits<T>::max
    static inline std::regex validator_{ R"(^[+-]?[0-9]{1,)" + std::to_string(std::numeric_limits<T>::max_digits10) +  R"(}$)" };
};

/**
 * Specialisation for unsigned integer types NOT supported by the JSON library
 *
 * FIXME untested due lack of compiler support for types larger than sizeof(nlohmann::json::number_unsigned_t)
 */
template <std::unsigned_integral T>
requires (sizeof(T) > sizeof(nlohmann::json::number_unsigned_t))
class JsonSerialiser<T> {
    public:
    static bool Validate(const nlohmann::json& serialised)
    {
        return serialised.type() == nlohmann::json::value_t::string && std::regex_match(serialised.get<std::string>(), validator_);
    }

    static nlohmann::json Serialise(const T& value)
    {
        std::stringstream valueStream;
        valueStream.precision(std::numeric_limits<T>::max_digits10);
        valueStream << value;
        return valueStream.str();
    }

    static T Deserialise(const nlohmann::json& serialised)
    {
        std::stringstream valueStream(serialised.get<std::string>());
        valueStream.precision(std::numeric_limits<T>::max_digits10);
        T value;
        valueStream >> value;
        return value;
    }

private:
    // Aiming for [sequence of 0-9, at least 1, at most std::numeric_limits<T>::max_digits10]
    // FIXME does not constrain values to between 0 and std::numeric_limits<T>::max
    static inline std::regex validator_{ R"(^[0-9]{1,)" + std::to_string(std::numeric_limits<T>::max_digits10) +  R"(}$)" };
};

/**
 * Specialisation for floating point types NOT supported by the JSON library
 *
 * FIXME untested due lack of compiler support for types larger than sizeof(nlohmann::json::number_unsigned_t)
 */
template <std::floating_point T>
requires (sizeof(T) > sizeof(nlohmann::json::number_float_t))
class JsonSerialiser<T> {
public:
    static bool Validate(const nlohmann::json& serialised)
    {
        return serialised.type() == nlohmann::json::value_t::string && std::regex_match(serialised.get<std::string>(), validator_);
    }

    static nlohmann::json Serialise(const T& value)
    {
        std::stringstream valueStream;
        valueStream.precision(std::numeric_limits<T>::max_digits10);
        valueStream << value;
        return valueStream.str();
    }

    static T Deserialise(const nlohmann::json& serialised)
    {
        std::stringstream valueStream(serialised.get<std::string>());
        valueStream.precision(std::numeric_limits<T>::max_digits10);
        T value;
        valueStream >> value;
        return value;
    }

private:
    static inline std::regex validator_{ R"(^([+-]?(?:[[:d:]]+\.?|[[:d:]]*\.[[:d:]]+))(?:[Ee][+-]?[[:d:]]+)?$)" };
};

template <typename T>
concept enumeration = std::is_enum_v<T>; // why isn't this in std?

template <enumeration T>
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
