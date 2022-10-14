#ifndef EASYSERDESSTDLIBSUPPORT_H
#define EASYSERDESSTDLIBSUPPORT_H

#include "EasySerDesClassHelper.h"

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
    static void SetupHelper(InternalHelper<std::pair<T1, T2>, T1, T2>& h)
    {
        h.RegisterConstruction(h.CreateParameter(&std::pair<T1, T2>::first),
                               h.CreateParameter(&std::pair<T1, T2>::second));
    }
};

template <typename... Ts>
class JsonSerialiser<std::tuple<Ts...>> : public JsonClassSerialiser<std::tuple<Ts...>, Ts...> {
public:
    // FIXME use "HelperType" when it supports templated types
    static void SetupHelper(InternalHelper<std::tuple<Ts...>, Ts...>& h)
    {
        SetupHelperInternal(h, std::make_index_sequence<sizeof...(Ts)>());
    }

private:
    template <std::size_t... Indexes>
    static void SetupHelperInternal(InternalHelper<std::tuple<Ts...>, Ts...>& h, std::index_sequence<Indexes...>)
    {
        // Naming them all T0 causes the helper to create unique names: T0, T1, T2...
        // The important part is that the last character of the duplicated label is a '0'
        h.RegisterConstruction(h.CreateParameter([](const std::tuple<Ts...>& t)
        {
            return std::get<Indexes>(t);
        }, "T0") ...);
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

// TODO a concept for "Type has a JsonSerialiser that extends JsonClassSerialiser"
// Then this can be used by shared and unique pointer, and could be used to allow for ranges that emplace_back

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
        return serialised.is_array() && std::ranges::all_of(serialised, [](const auto& item)
        {
            return JsonSerialiser<typename T::value_type>::Validate(item);
        });
    }

    static nlohmann::json Serialise(const T& range)
    {
        nlohmann::json serialisedItems = nlohmann::json::array();
        std::ranges::transform(range, std::back_inserter(serialisedItems), [](const auto& item)
        {
            return JsonSerialiser<typename T::value_type>::Serialise(item);
        });
        return serialisedItems;
    }

    static T Deserialise(const nlohmann::json& serialised)
    {
        T items;
        std::ranges::transform(serialised, std::inserter(items, items.end()), [](const auto& item)
        {
            return esd::DeserialiseWithoutChecks<typename T::value_type>(item);
        });
        return items;
    }
};

template <typename T, size_t N>
class JsonSerialiser<std::array<T, N>> {
public:
    static bool Validate(const nlohmann::json& serialised)
    {
        return serialised.is_array() && serialised.size() == N && std::ranges::all_of(serialised, [](const auto& item)
        {
            return esd::Validate<T>(item);
        });
    }

    static nlohmann::json Serialise(const std::array<T, N>& range)
    {
        nlohmann::json serialisedItems = nlohmann::json::array();
        std::ranges::transform(range, std::back_inserter(serialisedItems), [](const auto& item)
        {
            return esd::Serialise<T>(item);
        });
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


template <typename T>
concept TypeSupportedByEasySerDesViaClassHelper = IsSpecialisationOf<JsonSerialiser<T>, JsonClassSerialiser>;

template <typename T>
requires TypeSupportedByEasySerDes<T>
class JsonSerialiser<std::shared_ptr<T>> {
public:
    static bool Validate(const nlohmann::json& serialised)
    {
        bool valid = true;
        valid = valid && serialised.contains(wrappedTypeKey);
        return JsonSerialiser<T>::Validate(serialised.at(wrappedTypeKey));
    }

    static nlohmann::json Serialise(const std::shared_ptr<T>& shared)
    {
        nlohmann::json serialisedPtr = nlohmann::json::object();
        std::uintptr_t pointerValue = reinterpret_cast<std::uintptr_t>(shared.get());
        serialisedPtr[memoryAddressKey] = pointerValue;
        serialisedPtr[wrappedTypeKey] = JsonSerialiser<T>::Serialise(*shared);
        return serialisedPtr;
    }

    static std::shared_ptr<T> Deserialise(const nlohmann::json& serialised)
    {
        if constexpr (TypeSupportedByEasySerDesViaClassHelper<T>) {
            std::shared_ptr<T> ret = CheckCache(serialised);

            if (!ret) {
                ret = JsonSerialiser<T>::Deserialise(&std::make_shared<T, JsonSerialiser<T>::ConstructionArgsForwarder<std::make_shared, T>, serialised.at(wrappedTypeKey));

            }

            return ret;
        } else if constexpr (std::is_move_constructible_v<T> || std::is_copy_constructible_v<T>) {
            return std::make_shared<T>(esd::DeserialiseWithoutChecks<T>(serialised.at(wrappedTypeKey)));
        }
    }

private:
    struct PointerInfo {
        std::map<nlohmann::json, std::weak_ptr<T>> existingPointers;
    };

    static inline std::string memoryAddressKey = "ptr";
    static inline std::string wrappedTypeKey = "wrappedType";

    static inline std::map<std::uintptr_t, PointerInfo> existingPointers{};
    // TODO don't use this hack to register
    static inline int cacheClearRegister = []() -> std::map<T*, PointerInfo>
    {
        esd::CacheManager::AddEndOfOperationCallback([]()
        {
            existingPointers.clear();
        });
        return {};
    };

    // Weed out any invalid weak ptrs
    static void TrimCache()
    {
        for (PointerInfo& ptrInfo : existingPointers) {
            std::erase_if(ptrInfo.existingPointers, [](const auto& keyValuePair)
            {
                return keyValuePair.second.expired();
            });
        }
    }

    static std::shared_ptr<T> CheckCache(const nlohmann::json& serialised)
    {
        std::uintptr_t pointerValue = serialised.at(memoryAddressKey).get<std::uintptr_t>();
        if (existingPointers.contains(pointerValue)) {
            PointerInfo& ptrInfo = existingPointers.at(pointerValue);
            if (ptrInfo.existingPointers.contains(serialised)) {
                std::weak_ptr<T> weakPtr = ptrInfo.existingPointers.at(serialised);
                if (std::shared_ptr<T> ptr = weakPtr.lock(); ptr) {
                    return ptr;
                }
            }
        }
        return nullptr;
    }

    static void AddToCache(const nlohmann::json& serialised, const std::shared_ptr<T>& ptr)
    {
        std::uintptr_t pointerValue = reinterpret_cast<std::uintptr_t>(ptr.get());
        const nlohmann::json& serialisedValue = serialised.at(wrappedTypeKey);
        existingPointers[pointerValue].existingPointers[serialisedValue] = ptr;
    }
};

template <typename T>
requires TypeSupportedByEasySerDes<T>
class JsonSerialiser<std::unique_ptr<T>> {
public:
    static bool Validate(const nlohmann::json& serialised)
    {
        bool valid = true;
        valid = valid && serialised.contains(wrappedTypeKey);
        return JsonSerialiser<T>::Validate(serialised.at(wrappedTypeKey));
    }

    static nlohmann::json Serialise(const std::unique_ptr<T>& shared)
    {
        nlohmann::json serialisedPtr = nlohmann::json::object();
        serialisedPtr[wrappedTypeKey] = JsonSerialiser<T>::Serialise(*shared);
        return serialisedPtr;
    }

    static std::unique_ptr<T> Deserialise(const nlohmann::json& serialised)
    {
        if constexpr (TypeSupportedByEasySerDesViaClassHelper<T>) {
            return JsonSerialiser<T>::Deserialise(&std::make_shared<T, JsonSerialiser<T>::ConstructionArgsForwarder<std::make_unique, T>, serialised.at(wrappedTypeKey));
        } else if constexpr (std::is_move_constructible_v<T> || std::is_copy_constructible_v<T>) {
            return std::make_unique<T>(JsonSerialiser<T>::Deserialise(serialised.at(wrappedTypeKey)));
        }
    }

private:
    static inline std::string wrappedTypeKey = "wrappedType";
};

} // end namespace esd

#endif // EASYSERDESSTDLIBSUPPORT_H
