#ifndef EASYSERDESBUILTINTYPESUPPORT_H
#define EASYSERDESBUILTINTYPESUPPORT_H

#include "Core.h"
#include "JsonHelpers.h"

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
class Serialiser<bool> {
public:
    static bool Validate(Context& context, const nlohmann::json& serialised)
    {
        return serialised.is_boolean();
    }

    static void Serialise(DataWriter&& writer, const bool& value)
    {
        writer.SetFormatToValue();
        writer.Write(value);
    }

    static std::optional<bool> Deserialise(DataReader&& reader)
    {
        return reader.Read<bool>();
    }
};

/**
 * Constrained to signed integral types supported by the JSON library
 */
template <std::signed_integral T>
requires (sizeof(T) <= sizeof(nlohmann::json::number_integer_t))
class Serialiser<T> {
    public:
    static bool Validate(Context& context, const nlohmann::json& serialised)
    {
        using InternalType = nlohmann::json::number_integer_t;
        return MatchType(nlohmann::json::value_t::number_integer, serialised.type()) && serialised.get<InternalType>() == static_cast<InternalType>(Deserialise(context, serialised));
    }

    static void Serialise(DataWriter&& writer, const T& value)
    {
        writer.SetFormatToValue();
        writer.Write(value);
    }

    static std::optional<T> Deserialise(DataReader&& reader)
    {
        return reader.Read<T>();
    }
};

/**
 * Constrained to unsigned integral types supported by the JSON library
 */
template <std::unsigned_integral T>
requires (sizeof(T) <= sizeof(nlohmann::json::number_unsigned_t))
class Serialiser<T> {
    public:
    static bool Validate(Context& context, const nlohmann::json& serialised)
    {
        using InternalType = nlohmann::json::number_unsigned_t;
        return MatchType(nlohmann::json::value_t::number_unsigned, serialised.type()) && serialised.get<InternalType>() == static_cast<InternalType>(Deserialise(context, serialised));
    }

    static void Serialise(DataWriter&& writer, const T& value)
    {
        writer.SetFormatToValue();
        writer.Write(value);
    }

    static std::optional<T> Deserialise(DataReader&& reader)
    {
        return reader.Read<T>();
    }
};

/**
 * Constrained to floating point types supported by the JSON library
 */
template <std::floating_point T>
requires (sizeof(T) <= sizeof(nlohmann::json::number_float_t))
class Serialiser<T> {
public:
    static bool Validate(Context& context, const nlohmann::json& serialised)
    {
        using InternalType = nlohmann::json::number_float_t;
        return MatchType(nlohmann::json::value_t::number_float, serialised.type()) && serialised.get<InternalType>() == static_cast<InternalType>(Deserialise(context, serialised));
    }

    static void Serialise(DataWriter&& writer, const T& value)
    {
        writer.SetFormatToValue();
        writer.Write(value);
    }

    static std::optional<T> Deserialise(DataReader&& reader)
    {
        return reader.Read<T>();
    }
};

/**
 * Specialisation for signed integer types NOT supported by the JSON library
 *
 * FIXME untested due lack of compiler support for types larger than sizeof(nlohmann::json::number_integer_t)
 */
template <std::signed_integral T>
requires (sizeof(T) > sizeof(nlohmann::json::number_integer_t))
class Serialiser<T> {
    public:
    static bool Validate(Context& context, const nlohmann::json& serialised)
    {
        return serialised.type() == nlohmann::json::value_t::string && std::regex_match(serialised.get<std::string>(), validator_);
    }

    static void Serialise(DataWriter&& writer, const T& value)
    {
        std::stringstream valueStream;
        valueStream.precision(std::numeric_limits<T>::max_digits10);
        valueStream << value;
        writer.SetFormatToValue();
        writer.Write(valueStream.str());
    }

    static std::optional<T> Deserialise(DataReader&& reader)
    {
        std::optional<std::string> valueStr = reader.Read<std::string>();
        if (valueStr.has_value()) {
            std::stringstream valueStream(valueStr.value());
            valueStream.precision(std::numeric_limits<T>::max_digits10);
            T value;
            valueStream >> value;
            return value;
        }
        return std::nullopt;
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
class Serialiser<T> {
    public:
    static bool Validate(Context& context, const nlohmann::json& serialised)
    {
        return serialised.type() == nlohmann::json::value_t::string && std::regex_match(serialised.get<std::string>(), validator_);
    }

    static void Serialise(DataWriter&& writer, const T& value)
    {
        std::stringstream valueStream;
        valueStream.precision(std::numeric_limits<T>::max_digits10);
        valueStream << value;
        writer.SetFormatToValue();
        writer.Write(valueStream.str());
    }

    static std::optional<T> Deserialise(DataReader&& reader)
    {
        std::optional<std::string> valueStr = reader.Read<std::string>();
        if (valueStr.has_value()) {
            std::stringstream valueStream(valueStr.value());
            valueStream.precision(std::numeric_limits<T>::max_digits10);
            T value;
            valueStream >> value;
            return value;
        }
        return std::nullopt;
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
class Serialiser<T> {
public:
    static bool Validate(Context& context, const nlohmann::json& serialised)
    {
        return serialised.type() == nlohmann::json::value_t::string && std::regex_match(serialised.get<std::string>(), validator_);
    }

    static void Serialise(DataWriter&& writer, const T& value)
    {
        std::stringstream valueStream;
        valueStream.precision(std::numeric_limits<T>::max_digits10);
        valueStream << value;
        writer.SetFormatToValue();
        writer.Write(valueStream.str());
    }

    static std::optional<T> Deserialise(DataReader&& reader)
    {
        std::optional<std::string> valueStr = reader.Read<std::string>();
        if (valueStr.has_value()) {
            std::stringstream valueStream(valueStr.value());
            valueStream.precision(std::numeric_limits<T>::max_digits10);
            T value;
            valueStream >> value;
            return value;
        }
        return std::nullopt;
    }

private:
    static inline std::regex validator_{ R"(^([+-]?(?:[[:d:]]+\.?|[[:d:]]*\.[[:d:]]+))(?:[Ee][+-]?[[:d:]]+)?$)" };
};

template <typename T>
concept enumeration = std::is_enum_v<T>; // why isn't this in std?

template <enumeration T>
class Serialiser<T> {
public:
    static bool Validate(Context& context, const nlohmann::json& serialised)
    {
        return Serialiser<std::underlying_type_t<T>>::Validate(context, serialised);
    }

    static void Serialise(DataWriter&& writer, const T& value)
    {
        writer.SetFormatToValue();
        writer.Write(std::bit_cast<std::underlying_type_t<T>>(value));
    }

    static std::optional<T> Deserialise(DataReader&& reader)
    {
        std::optional<std::underlying_type_t<T>> underlying = reader.Read<std::underlying_type_t<T>>();
        if (underlying.has_value()) {
            return std::bit_cast<T>(underlying.value());
        }
        return std::nullopt;
    }
};

} // end namespace esd

#endif // EASYSERDESBUILTINTYPESUPPORT_H
