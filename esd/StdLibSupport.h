#ifndef EASYSERDESSTDLIBSUPPORT_H
#define EASYSERDESSTDLIBSUPPORT_H

#include "ClassHelper.h"
#include "PolymorphicClassHelper.h"

#include <nlohmann/json.hpp>

#include <optional>
#include <sstream>
#include <ranges>

/**
 * This file is for implementing support for std library constructs
 */

namespace esd {

// Specialise for std::byte so that is is more user readable in JSON form
template <>
class JsonSerialiser<std::byte> {
public:
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

template <typename T1, typename T2>
class JsonSerialiser<std::pair<T1, T2>> : public JsonClassSerialiser<std::pair<T1, T2>, T1, T2> {
public:
    // FIXME use "HelperType" when it supports templated types
    static void Configure()
    {
        // FIXME comment on this name lookup failure in the documentation (appears to only affect support for templated types)
        JsonSerialiser::SetConstruction(JsonSerialiser::CreateParameter(&std::pair<T1, T2>::first),
                                        JsonSerialiser::CreateParameter(&std::pair<T1, T2>::second));
    }
};

template <typename... Ts>
class JsonSerialiser<std::tuple<Ts...>> : public JsonClassSerialiser<std::tuple<Ts...>, Ts...> {
public:
    // FIXME use "HelperType" when it supports templated types
    static void Configure()
    {
        ConfigureInternal(std::make_index_sequence<sizeof...(Ts)>());
    }

private:
    template <std::size_t... Indexes>
    static void ConfigureInternal(std::index_sequence<Indexes...>)
    {
        JsonSerialiser::SetConstruction(JsonSerialiser::CreateParameter([](const std::tuple<Ts...>& t)
        {
            return std::get<Indexes>(t);
        }, "T" + std::to_string(Indexes)) ...);
    }
};

template <>
class JsonSerialiser<std::string> {
public:
    static bool Validate(const nlohmann::json& serialised)
    {
        return serialised.is_string();
    }

    static nlohmann::json Serialise(const std::string& stringLikeValue)
    {
        return stringLikeValue;
    }

    static std::string Deserialise(const nlohmann::json& serialised)
    {
        return serialised.get<std::string>();
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
class JsonSerialiser<T> {
public:
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
class JsonSerialiser<T> {
public:
    static bool Validate(const nlohmann::json& serialised)
    {
        return serialised.is_array() && std::ranges::all_of(serialised, esd::Validate<typename T::value_type>);
    }

    static nlohmann::json Serialise(const T& range)
    {
        nlohmann::json serialisedItems = nlohmann::json::array();
        std::ranges::transform(range, std::back_inserter(serialisedItems), &esd::Serialise<typename T::value_type>);
        return serialisedItems;
    }

    static T Deserialise(const nlohmann::json& serialised)
    {
        T items;
        std::ranges::transform(serialised, std::inserter(items, items.end()), esd::DeserialiseWithoutChecks<typename T::value_type>);
        return items;
    }
};

template <typename T, size_t N>
class JsonSerialiser<std::array<T, N>> {
public:
    static bool Validate(const nlohmann::json& serialised)
    {
        return serialised.is_array() && serialised.size() == N && std::ranges::all_of(serialised, esd::Validate<T>);
    }

    static nlohmann::json Serialise(const std::array<T, N>& range)
    {
        nlohmann::json serialisedItems = nlohmann::json::array();
        std::ranges::transform(range, std::back_inserter(serialisedItems), esd::Serialise<T>);
        return serialisedItems;
    }

    static std::array<T, N> Deserialise(const nlohmann::json& serialised)
    {
        return CreateArray(serialised, std::make_index_sequence<N>());
    }

private:
    template <size_t... I>
    static std::array<T, N> CreateArray(const nlohmann::json& serialised, std::index_sequence<I...>)
    {
        return std::array<T, N>{ esd::DeserialiseWithoutChecks<T>(serialised.at(I)) ... };
    }
};

// FIXME the following concepts mean that knowledge of the Class and PolymorphicClass helpers is leaking out of those headers
// Ideally support for those types would be completely encapsulated within their own headers

template <typename T>
concept TypeSupportedByEasySerDesViaClassHelper = IsDerivedFromSpecialisationOf<JsonSerialiser<T>, JsonClassSerialiser>;

template <typename T>
concept TypeSupportedByEasySerDesViaPolymorphicClassHelper = IsDerivedFromSpecialisationOf<JsonSerialiser<T>, JsonPolymorphicClassSerialiser>;

template <typename T>
requires TypeSupportedByEasySerDesViaClassHelper<T>
      || TypeSupportedByEasySerDesViaPolymorphicClassHelper<T>
      || requires (T, nlohmann::json j) { { std::make_shared<T>(esd::DeserialiseWithoutChecks<T>(j)) } -> std::same_as<std::shared_ptr<T>>; }
class JsonSerialiser<std::shared_ptr<T>> {
public:
    static bool Validate(const nlohmann::json& serialised)
    {
        bool valid = serialised.contains(wrappedTypeKey);

        if constexpr (TypeSupportedByEasySerDesViaPolymorphicClassHelper<T>) {
            return valid && JsonSerialiser<T>::ValidatePolymorphic(serialised.at(wrappedTypeKey));
        } else {
            return valid && JsonSerialiser<T>::Validate(serialised.at(wrappedTypeKey));
        }
    }

    static nlohmann::json Serialise(const std::shared_ptr<T>& shared)
    {
        nlohmann::json serialisedPtr = nlohmann::json::object();
        std::uintptr_t pointerValue = reinterpret_cast<std::uintptr_t>(shared.get());
        serialisedPtr[uniqueIdentifierKey] = pointerValue;
        if constexpr (TypeSupportedByEasySerDesViaPolymorphicClassHelper<T>) {
            serialisedPtr[wrappedTypeKey] = JsonSerialiser<T>::SerialisePolymorphic(*shared);
        } else {
            serialisedPtr[wrappedTypeKey] = JsonSerialiser<T>::Serialise(*shared);
        }
        return serialisedPtr;
    }

    static std::shared_ptr<T> Deserialise(const nlohmann::json& serialised)
    {
        std::shared_ptr<T> ret = CheckCache(serialised);
        if (!ret) {
            if constexpr (TypeSupportedByEasySerDesViaPolymorphicClassHelper<T>) {
                ret = JsonSerialiser<T>::DeserialisePolymorphic(serialised.at(wrappedTypeKey));
            } else if constexpr (TypeSupportedByEasySerDesViaClassHelper<T>) {
                ret = JsonSerialiser<T>::Deserialise([](auto... args){ return std::make_shared<T>(args...); }, serialised.at(wrappedTypeKey));
            } else if constexpr (std::is_move_constructible_v<T> || std::is_copy_constructible_v<T>) {
                ret = std::make_shared<T>(esd::DeserialiseWithoutChecks<T>(serialised.at(wrappedTypeKey)));
            }
            AddToCache(serialised, ret);
        }
        return ret;
    }

private:
    struct PointerInfo {
        // Stored values may have been modified offline, so cache a different pointer for
        std::map<std::string, std::weak_ptr<T>> variations;
    };

    static inline std::string uniqueIdentifierKey = "ptr";
    static inline std::string wrappedTypeKey = "wrappedType";

    static inline std::map<std::uintptr_t, PointerInfo> cachedPointers_{};

    static std::shared_ptr<T> CheckCache(const nlohmann::json& serialised)
    {
        std::uintptr_t pointerValue = serialised.at(uniqueIdentifierKey).get<std::uintptr_t>();
        if (cachedPointers_.contains(pointerValue)) {
            PointerInfo& ptrInfo = cachedPointers_.at(pointerValue);
            std::string key = serialised.at(wrappedTypeKey).dump();
            if (ptrInfo.variations.contains(key)) {
                std::weak_ptr<T> weakPtr = ptrInfo.variations.at(key);
                if (std::shared_ptr<T> ptr = weakPtr.lock(); ptr) {
                    return ptr;
                }
            }
        }
        return nullptr;
    }

    static void AddToCache(const nlohmann::json& serialised, const std::shared_ptr<T>& ptr)
    {
        // TODO perhaps there is a nicer way to do this?
        static bool cacheClearCallbackRegistered = false;
        if (!cacheClearCallbackRegistered) {
            esd::CacheManager::AddEndOfOperationCallback([]()
            {
                cachedPointers_.clear();
            });
            cacheClearCallbackRegistered = true;
        }

        std::uintptr_t pointerValue = serialised.at(uniqueIdentifierKey).get<std::uintptr_t>();
        const nlohmann::json& serialisedValue = serialised.at(wrappedTypeKey);
        cachedPointers_[pointerValue].variations[serialisedValue.dump()] = ptr;
    }
};

template <typename T>
requires TypeSupportedByEasySerDesViaClassHelper<T>
      || TypeSupportedByEasySerDesViaPolymorphicClassHelper<T>
      || requires (T, nlohmann::json j) { { std::make_unique<T>(esd::DeserialiseWithoutChecks<T>(j)) } -> std::same_as<std::unique_ptr<T>>; }
class JsonSerialiser<std::unique_ptr<T>> {
public:
    static bool Validate(const nlohmann::json& serialised)
    {
        bool valid = serialised.contains(wrappedTypeKey);

        if constexpr (TypeSupportedByEasySerDesViaPolymorphicClassHelper<T>) {
            return valid && JsonSerialiser<T>::ValidatePolymorphic(serialised.at(wrappedTypeKey));
        } else {
            return valid && JsonSerialiser<T>::Validate(serialised.at(wrappedTypeKey));
        }
    }

    static nlohmann::json Serialise(const std::unique_ptr<T>& shared)
    {
        nlohmann::json serialisedPtr = nlohmann::json::object();

        if constexpr (TypeSupportedByEasySerDesViaPolymorphicClassHelper<T>) {
            serialisedPtr[wrappedTypeKey] = JsonSerialiser<T>::SerialisePolymorphic(*shared);
        } else {
            serialisedPtr[wrappedTypeKey] = JsonSerialiser<T>::Serialise(*shared);
        }

        return serialisedPtr;
    }

    static std::unique_ptr<T> Deserialise(const nlohmann::json& serialised)
    {
        if constexpr (TypeSupportedByEasySerDesViaPolymorphicClassHelper<T>) {
            return JsonSerialiser<T>::DeserialisePolymorphic(serialised.at(wrappedTypeKey));
        } else if constexpr (TypeSupportedByEasySerDesViaClassHelper<T>) {
            return JsonSerialiser<T>::Deserialise([](auto... args){ return std::make_unique<T>(args...); }, serialised.at(wrappedTypeKey));
        } else if constexpr (std::is_move_constructible_v<T> || std::is_copy_constructible_v<T>) {
            return std::make_unique<T>(esd::DeserialiseWithoutChecks<T>(serialised.at(wrappedTypeKey)));
        }
    }

private:
    static inline std::string wrappedTypeKey = "wrappedType";
};

} // end namespace esd

#endif // EASYSERDESSTDLIBSUPPORT_H
