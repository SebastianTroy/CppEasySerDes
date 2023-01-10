#ifndef EASYSERDESSTDLIBSUPPORT_H
#define EASYSERDESSTDLIBSUPPORT_H

#include "ClassHelper.h"
#include "PolymorphismHelper.h"

#include <nlohmann/json.hpp>

#include <optional>
#include <variant>
#include <sstream>
#include <ranges>
#include <set>

/**
 * This file is for implementing support for std library constructs
 */

namespace esd {

// Specialise for std::byte so that is is more user readable in JSON form
template <>
class Serialiser<std::byte> {
public:
    static bool Validate(Context& context, const nlohmann::json& serialised)
    {
        return serialised.is_string() && serialised.get<std::string>().size() == 4 && serialised.get<std::string>().starts_with("0x");
    }

    static nlohmann::json Serialise(Context& context, const std::byte& value)
    {
        // TODO when std::format available, this will be a lot cleaner
        std::ostringstream byteStringStream;
        // Stream uint32 because it won't be converted to a char
        uint32_t valueAsWord = std::bit_cast<uint8_t>(value);
        byteStringStream << "0x" << std::hex << std::setfill('0') << std::fixed << std::setw(2) << valueAsWord;
        return byteStringStream.str();
    }

    static std::byte Deserialise(Context& context, const nlohmann::json& serialised)
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
class Serialiser<std::pair<T1, T2>> : public ClassHelper<std::pair<T1, T2>, T1, T2> {
public:
    static void Configure()
    {
        // name lookup needs help with templated types
        using This = Serialiser<std::pair<T1, T2>>;
        This::SetConstruction(This::CreateParameter(&std::pair<T1, T2>::first),
                              This::CreateParameter(&std::pair<T1, T2>::second));
    }
};

template <typename... Ts>
class Serialiser<std::tuple<Ts...>> : public ClassHelper<std::tuple<Ts...>, Ts...> {
public:
    static void Configure()
    {
        ConfigureInternal(std::make_index_sequence<sizeof...(Ts)>());
    }

private:
    template <std::size_t... Indexes>
    static void ConfigureInternal(std::index_sequence<Indexes...>)
    {
        Serialiser::SetConstruction(Serialiser::CreateParameter([](const std::tuple<Ts...>& t)
        {
            return std::get<Indexes>(t);
        }, "T" + std::to_string(Indexes) + detail::TypeNameStr<Ts>()) ...);
    }
};

template <>
class Serialiser<std::string> {
public:
    static bool Validate(Context& context, const nlohmann::json& serialised)
    {
        return serialised.is_string();
    }

    static nlohmann::json Serialise(Context& context, const std::string& string)
    {
        return string;
    }

    static std::string Deserialise(Context& context, const nlohmann::json& serialised)
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
class Serialiser<T> {
public:
    static bool Validate(Context& context, const nlohmann::json& serialised)
    {
        return serialised.is_string();
    }

    static nlohmann::json Serialise(Context& context, const T& stringLikeValue)
    {
        std::string serialised;
        std::ranges::copy(stringLikeValue, std::back_inserter(serialised));
        return serialised;
    }

    static T Deserialise(Context& context, const nlohmann::json& serialised)
    {
        T items;
        std::ranges::copy(serialised.get<std::string>(), std::back_inserter(items));
        return items;
    }
};

template <std::ranges::range T>
class Serialiser<T> {
public:
    static bool Validate(Context& context, const nlohmann::json& serialised)
    {
        return serialised.is_array() && std::ranges::all_of(serialised, [&](const nlohmann::json& value){ return Serialiser<typename T::value_type>::Validate(context, value); } );
    }

    static nlohmann::json Serialise(Context& context, const T& range)
    {
        nlohmann::json serialisedItems = nlohmann::json::array();
        std::ranges::transform(range, std::back_inserter(serialisedItems), [&](const auto& value){ return Serialiser<typename T::value_type>::Serialise(context, value); });
        return serialisedItems;
    }

    static T Deserialise(Context& context, const nlohmann::json& serialised)
    {
        T items;
        std::ranges::transform(serialised, std::inserter(items, items.end()), [&](const nlohmann::json& value){ return Serialiser<typename T::value_type>::Deserialise(context, value); });
        return items;
    }
};

template <size_t N>
class Serialiser<std::bitset<N>> {
public:
    static bool Validate(Context& context, const nlohmann::json& serialised)
    {
        bool valid = serialised.is_string();

        if (valid) {
            std::string bitsString = serialised.get<std::string>();
            valid = bitsString.size() == N;
            valid = valid && std::ranges::all_of(bitsString, [](const char& c) -> bool
                                                             {
                                                                 return c == '0' || c == '1';
                                                             });
        }
        return valid;
    }

    static nlohmann::json Serialise(Context& context, const std::bitset<N>& range)
    {
        return range.to_string();
    }

    static std::bitset<N> Deserialise(Context& context, const nlohmann::json& serialised)
    {
        return std::bitset<N>(serialised.get<std::string>());
    }
};

template <typename T, size_t N>
class Serialiser<std::array<T, N>> {
public:
    static bool Validate(Context& context, const nlohmann::json& serialised)
    {
        return serialised.is_array() && serialised.size() == N && std::ranges::all_of(serialised, [&](const auto& value){ return Serialiser<T>::Validate(context, value); });
    }

    static nlohmann::json Serialise(Context& context, const std::array<T, N>& range)
    {
        nlohmann::json serialisedItems = nlohmann::json::array();
        std::ranges::transform(range, std::back_inserter(serialisedItems), [&](const auto& value){ return Serialiser<T>::Serialise(context, value); });
        return serialisedItems;
    }

    static std::array<T, N> Deserialise(Context& context, const nlohmann::json& serialised)
    {
        return CreateArray(context, serialised, std::make_index_sequence<N>());
    }

private:
    template <size_t... I>
    static std::array<T, N> CreateArray(Context& context, const nlohmann::json& serialised, std::index_sequence<I...>)
    {
        return std::array<T, N>{ Serialiser<T>::Deserialise(context, serialised.at(I)) ... };
    }
};

template <typename T>
class Serialiser<std::optional<T>> {
public:
    static bool Validate(Context& context, const nlohmann::json& serialised)
    {
        return (serialised == nullString) || Serialiser<T>::Validate(context, serialised);
    }

    static nlohmann::json Serialise(Context& context, const std::optional<T>& toSerialise)
    {
        nlohmann::json serialisedOptional = nullString;

        if (toSerialise.has_value()) {
            serialisedOptional = Serialiser<T>::Serialise(context, toSerialise.value());
        }

        return serialisedOptional;
    }

    static std::optional<T> Deserialise(Context& context, const nlohmann::json& serialised)
    {
        std::optional<T> ret = std::nullopt;

        if (serialised != nullString) {
            if constexpr (HasClassHelperSpecialisation<T>) {
                ret = Serialiser<T>::DeserialiseInPlace(context, [](auto... args){ return std::optional<T>::emplace(args...); }, serialised);
            } else if constexpr (std::is_move_constructible_v<T> || std::is_copy_constructible_v<T>) {
                ret = std::optional<T>(Serialiser<T>::Deserialise(context, serialised));
            }
        }

        return ret;
    }

private:
    static inline const std::string nullString = "std::nullopt";
};

template <typename... Ts>
class Serialiser<std::variant<Ts...>> {
public:
    static bool Validate(Context& context, const nlohmann::json& serialised)
    {
        bool valid = serialised.type() == nlohmann::json::value_t::object && serialised.size() == sizeof...(Ts);

        // The following immediately called lambda is called once for each type
        valid = (... && [&]() -> bool
        {
            std::string key = detail::TypeNameStr<Ts>();
            return serialised.contains(key) && ((serialised[key] == nullString) || Serialiser<Ts>::Validate(context, serialised[key]));
        }());

        return valid;
    }

    static nlohmann::json Serialise(Context& context, const std::variant<Ts...>& toSerialise)
    {
        nlohmann::json serialisedVariant;

        // The following immediately called lambda is called once for each type
        ([&]()
        {
            std::string key = detail::TypeNameStr<Ts>();
            if (std::holds_alternative<Ts>(toSerialise)) {
                serialisedVariant[key] = Serialiser<Ts>::Serialise(context, std::get<Ts>(toSerialise));
            } else {
                serialisedVariant[key] = nullString;
            }
        }(), ...);

        return serialisedVariant;
    }

    static std::variant<Ts...> Deserialise(Context& context, const nlohmann::json& serialised)
    {
        std::variant<Ts...> ret;

        // The following immediately called lambda is called once for each type
        ([&]()
        {
            std::string key = detail::TypeNameStr<Ts>();
            if (serialised[key] != nullString) {
                ret = Serialiser<Ts>::Deserialise(context, serialised[key]);
            }
        }(), ...);

        return ret;
    }

private:
    static inline const std::string nullString = "nullVariant";
};

template <typename T>
requires HasClassHelperSpecialisation<T>
      || requires (esd::Context& context, T, nlohmann::json j) { { std::make_unique<T>(Serialiser<T>::Deserialise(context, j)) } -> std::same_as<std::unique_ptr<T>>; }
      || (std::is_abstract_v<T> && HasPolymorphismHelperSpecialisation<T>)
class Serialiser<std::unique_ptr<T>> {
public:
    static bool Validate(Context& context, const nlohmann::json& serialised)
    {
        bool valid = false;

        if (serialised == nullPointerString) {
            valid = true;
        } else if constexpr (HasPolymorphismHelperSpecialisation<T>) {
            valid = PolymorphismHelper<T>::ValidatePolymorphic(context, serialised);
        } else {
            valid = Serialiser<T>::Validate(context, serialised);
        }

        return valid;
    }

    static nlohmann::json Serialise(Context& context, const std::unique_ptr<T>& toSerialise)
    {
        // Delegate to raw pointer function so that shared_ptr<T> can share the impl
        return Serialise(context, toSerialise.get());
    }

    static std::unique_ptr<T> Deserialise(Context& context, const nlohmann::json& serialised)
    {
        if (serialised == nullPointerString) {
            return nullptr;
        }

        if constexpr (HasPolymorphismHelperSpecialisation<T>) {
            /*
             * PolymorphicHelper calls back into `
             * Serialiser<std::unique_ptr<DerivedType>>`, so the `IsDerivedType`
             * check is important here to prevent infinite recursive calls.
             */
            if (PolymorphismHelper<T>::ContainsPolymorphicType(serialised)) {
                return PolymorphismHelper<T>::DeserialisePolymorphic(context, serialised);
            } else if constexpr (std::is_abstract_v<T>) {
                context.LogError("Cannot deserialise an abstract type. Expected a non-abstract child type of " + detail::TypeNameStr<T>());
                return nullptr;
            }
        }

        if constexpr (HasClassHelperSpecialisation<T>) {
            return Serialiser<T>::DeserialiseInPlace(context, [](auto... args){ return std::make_unique<T>(args...); }, serialised);
        } else if constexpr (TypeSupportedByEasySerDes<T>) {
            return std::make_unique<T>(Serialiser<T>::Deserialise(context, serialised));
        }
    }

private:
    static inline const std::string nullPointerString = "nullptr";

    friend Serialiser<std::shared_ptr<T>>;
    static nlohmann::json Serialise(Context& context, const T* const toSerialise)
    {
        nlohmann::json serialised;

        if (toSerialise == nullptr) {
            serialised = nullPointerString;
        } else if constexpr (HasPolymorphismHelperSpecialisation<T>) {
            serialised = PolymorphismHelper<T>::SerialisePolymorphic(context, *toSerialise);
        } else {
            serialised = Serialiser<T>::Serialise(context, *toSerialise);
        }

        return serialised;
    }
};

/**
 * the shared_ptr specialisation basically just keeps track of shared'ness and
 * delegates to the unique_ptr specialisation for the rest.
 */
template <typename T>
requires HasClassHelperSpecialisation<T>
      || requires (T, Context& c, nlohmann::json j) { { std::make_shared<T>(Serialiser<T>::Deserialise(c, j)) } -> std::same_as<std::shared_ptr<T>>; }
      || (std::is_abstract_v<T> && HasPolymorphismHelperSpecialisation<T>)
class Serialiser<std::shared_ptr<T>> {
public:
    static inline const std::string wrappedTypeKey = "wrappedType";

    static bool Validate(Context& context, const nlohmann::json& serialised)
    {
        bool valid = serialised.contains(wrappedTypeKey)
                  && serialised.contains(uniqueIdentifierKey)
                  && Serialiser<std::unique_ptr<T>>::Validate(context, serialised.at(wrappedTypeKey)); // Delegate to unique_ptr to avoid code duplication
        return valid;
    }

    static nlohmann::json Serialise(Context& context, const std::shared_ptr<T>& toSerialise)
    {
        nlohmann::json serialisedPtr = nlohmann::json::object();
        std::uintptr_t pointerValue = reinterpret_cast<std::uintptr_t>(toSerialise.get());
        serialisedPtr[uniqueIdentifierKey] = pointerValue;
        // Can't delegate to unique_ptr to avoid code duplication
        serialisedPtr[wrappedTypeKey] = Serialiser<std::unique_ptr<T>>::Serialise(context, toSerialise.get());
        return serialisedPtr;
    }

    static std::shared_ptr<T> Deserialise(Context& context, const nlohmann::json& serialised)
    {
        std::shared_ptr<T> ret = CheckCache(context, serialised);
        if (!ret) {
            // Delegate to unique_ptr to avoid code duplication
            // TODO this might have performance implications that could be solved by using make_shared directly
            ret = Serialiser<std::unique_ptr<T>>::Deserialise(context, serialised.at(wrappedTypeKey));
            AddToCache(context, serialised, ret);
        }
        return ret;
    }

private:
    struct PointerInfo {
        // Stored values may have been modified offline, so cache a different pointer for
        std::map<std::string, std::weak_ptr<T>> variations;
    };

    using CacheType = std::map<std::uintptr_t, PointerInfo>;

    static inline std::string uniqueIdentifierKey = "ptr";
    static inline std::string cacheName = "shared_ptr";

    static std::shared_ptr<T> CheckCache(Context& context, const nlohmann::json& serialised)
    {
        std::uintptr_t pointerValue = serialised.at(uniqueIdentifierKey).get<std::uintptr_t>();
        auto& cachedPointers = context.GetCache<CacheType>(cacheName);
        if (cachedPointers.contains(pointerValue)) {
            PointerInfo& ptrInfo = cachedPointers.at(pointerValue);
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

    static void AddToCache(Context& context, const nlohmann::json& serialised, const std::shared_ptr<T>& toCache)
    {
        std::uintptr_t pointerValue = serialised.at(uniqueIdentifierKey).get<std::uintptr_t>();
        const nlohmann::json& serialisedValue = serialised.at(wrappedTypeKey);
        CacheType& cachedPointers = context.GetCache<CacheType>(cacheName);
        cachedPointers[pointerValue].variations[serialisedValue.dump()] = toCache;
    }
};

} // end namespace esd

#endif // EASYSERDESSTDLIBSUPPORT_H
