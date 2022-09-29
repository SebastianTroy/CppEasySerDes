#ifndef EASYSERDESSTDLIBSUPPORT_H
#define EASYSERDESSTDLIBSUPPORT_H

#include "EasySerDesCore.h"

#include <nlohmann/json.hpp>

#include <optional>
#include <sstream>
#include <ranges>

/**
 * This file is for implementing support for std library constructs
 */

namespace util {


// Specialise for std::byte so that is is more user readable in JSON form
template <>
struct JsonSerialiser<std::byte> {
    static bool Validate(const nlohmann::json& serialised)
    {
        return serialised.is_string() && serialised.get<std::string>().size() == 2;
    }

    static nlohmann::json Serialise(const std::byte& value)
    {
        // TODO when std::format available, this will be a lot cleaner
        std::ostringstream byteStringStream;
        // Stream uint32 because it won't be converted to a char
        uint32_t valueAsWord = std::bit_cast<uint8_t>(value);
        byteStringStream << std::hex << std::setfill('0') << std::fixed << std::setw(2) << valueAsWord;
        return byteStringStream.str();
    }

    static std::byte Deserialise(const nlohmann::json& serialised)
    {
        // TODO when std::format available, this will be a lot cleaner
        uint32_t deserialised;
        std::istringstream byteStringStream(serialised.get<std::string>());
        byteStringStream >> std::hex >> deserialised;
        uint8_t deserialisedAsByte = deserialised & 0xFF;
        return std::bit_cast<std::byte>(deserialisedAsByte);
    }
};

// TODO perhaps support Pair using the class helper (once implemented), might be less verbose
template <typename T1, typename T2>
struct JsonSerialiser<std::pair<T1, T2>> {
    static bool Validate(const nlohmann::json& serialised)
    {
        return serialised.is_array() && serialised.size() == 2
                && JsonSerialiser<T1>::Validate(serialised.at(0))
                && JsonSerialiser<T2>::Validate(serialised.at(1));
    }

    static nlohmann::json Serialise(const std::pair<T1, T2>& value)
    {
        nlohmann::json serialisedValues = nlohmann::json::array();
        serialisedValues.push_back(JsonSerialiser<T1>::Serialise(value.first));
        serialisedValues.push_back(JsonSerialiser<T2>::Serialise(value.second));
        return serialisedValues;
    }

    static std::pair<T1, T2> Deserialise(const nlohmann::json& serialised)
    {
        return std::make_pair(JsonSerialiser<T1>::Deserialise(serialised.at(0)), JsonSerialiser<T2>::Deserialise(serialised.at(1)));
    }
};

template <typename... Ts>
struct JsonSerialiser<std::tuple<Ts...>> {
    static bool Validate(const nlohmann::json& serialised)
    {
        size_t index = 0;
        return serialised.is_array() && serialised.size() == sizeof...(Ts)
                && (JsonSerialiser<Ts>::Validate(serialised.at(index++)) && ...);
    }

    static nlohmann::json Serialise(const std::tuple<Ts...>& value)
    {
        nlohmann::json serialisedValues = nlohmann::json::array();
        std::apply([&](const auto&... tupleItems)
        {
            (serialisedValues.push_back(JsonSerialiser<std::remove_cvref_t<decltype(tupleItems)>>::Serialise(tupleItems)), ...);
        }, value);
        return serialisedValues;
    }

    static std::tuple<Ts...> Deserialise(const nlohmann::json& serialised)
    {
        size_t index = 0;
        return { [&]{ return JsonSerialiser<Ts>::Deserialise(serialised.at(index++)); }()... };
    }
};

template <typename T>
concept InsertableConcept = requires (T c, typename T::value_type v) { c.insert(v); };

template <typename T>
concept PushBackableConcept = requires (T c, typename T::value_type v) { c.push_back(v); };

// Specialise for collections of chars so that is is more user readable in JSON form
template <typename T>
concept StringLikeConcept = std::ranges::range<T> && std::same_as<typename T::value_type, char> && PushBackableConcept<T>;

template <StringLikeConcept T>
struct JsonSerialiser<T> {
    static bool Validate(const nlohmann::json& serialised)
    {
        return serialised.is_string();
    }

    static nlohmann::json Serialise(const T& stringLikeValue)
    {
        std::string serialised;
        std::ranges::copy(stringLikeValue, std::back_inserter(serialised));
        return serialised;
    }

    static T Deserialise(const nlohmann::json& serialised)
    {
        T items;
        std::ranges::copy(serialised.get<std::string>(), std::back_inserter(items));
        return items;
    }
};

template <std::ranges::range T>
struct JsonSerialiser<T> {
    static bool Validate(const nlohmann::json& serialised)
    {
        return serialised.is_array() && std::ranges::all_of(serialised, [](const auto& item)
                                                                        {
                                                                            return JsonSerialiser<std::remove_cvref_t<typename T::value_type>>::Validate(item);
                                                                        });
    }

    static nlohmann::json Serialise(const T& range)
    {
        nlohmann::json serialisedItems = nlohmann::json::array();
        std::ranges::transform(range, std::back_inserter(serialisedItems), [](const auto& item)
        {
            return JsonSerialiser<std::remove_cvref_t<typename T::value_type>>::Serialise(item);
        });
        return serialisedItems;
    }

    static T Deserialise(const nlohmann::json& serialised)
    {
        T items;
        std::ranges::transform(serialised, std::inserter(items, items.end()), [](const auto& item)
        {
            return JsonSerialiser<std::remove_cvref_t<typename T::value_type>>::Deserialise(item);
        });
        return items;
    }
};

// TODO support shared_ptr, also save the memory address and some session UID so that it can be mapped and subsequent pointers to the same address can be copied rather than re-deserialised
// (actually maybe they should be checked for sameness first, as someone may have edited the file and genuinely want one of the previously shared values to be different...)
// Still want to share as many as possible for memory efficiency though
// ACTUALLY perhaps instead in the validate it should fail a shared pointer that is different to an existing deserialised version of itself, and deserialise should forego the check for speed

// TODO too much variance in how these array like types define their size and value type (bitset doesn't have value_type etc)
//template <typename T>
//concept ArrayOperatableConcept = requires (T a) {
//    { a[0] } -> std::same_as<typename T::value_type>;
//    { a.size() } -> std::same_as<size_t>;
//};

//template <ArrayOperatableConcept T>
//struct JsonSerialiser<T> {
//    static bool Validate(const nlohmann::json& serialised)
//    {
//    }

//    static nlohmann::json Serialise(const T& range)
//    {
//    }

//    static T Deserialise(const nlohmann::json& serialised)
//    {
//    }
//};

} // end namespace util

#endif // EASYSERDESSTDLIBSUPPORT_H
