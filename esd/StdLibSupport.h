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

    static void Serialise(DataWriter&& writer, const std::string& string)
    {
        writer.SetFormatToValue();
        writer.Write(string);
    }

    static std::optional<std::string> Deserialise(DataReader&& reader)
    {
        return reader.Read<std::string>();
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

    static void Serialise(DataWriter&& writer, const T& stringLikeValue)
    {
        std::string serialised;
        std::ranges::copy(stringLikeValue, std::back_inserter(serialised));
        writer.SetFormatToValue();
        writer.Write(serialised);
    }

    static std::optional<T> Deserialise(DataReader&& reader)
    {
        std::optional<std::string> dataAsString = reader.Read<std::string>();
        if (!dataAsString) {
            return std::nullopt;
        }

        T items;
        if constexpr (requires (T t, std::size_t c){ { t.reserve(c) } -> std::same_as<void>; }) {
            items.reserve(dataAsString->size());
        }
        std::ranges::copy(*dataAsString, std::back_inserter(items));
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

    static void Serialise(DataWriter&& writer, const T& range)
    {
        // TODO add writer.WriteRange() to make Serialise and Deserialise symmetric
        writer.SetFormatToArray(range.size());
        std::ranges::for_each(range, [&](const auto& value){ writer.PushBack(value); });
    }

    static std::optional<T> Deserialise(DataReader&& reader)
    {
        return reader.ReadRange<T>();
    }
};

template <std::size_t N>
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

    static void Serialise(DataWriter&& writer, const std::bitset<N>& range)
    {
        writer.SetFormatToValue();
        writer.Write(range.to_string());
    }

    static std::optional<std::bitset<N>> Deserialise(DataReader&& reader)
    {
        auto bitsString = reader.Read<std::string>();
        if (!bitsString) {
            return std::nullopt;
        }

        if (bitsString->size() != N) {
            reader.LogError("Expected exactly " + std::to_string(N) + " bits of data, but got " + *bitsString + " (" + std::to_string(bitsString->size()) + " bits).");
            return std::nullopt;
        }

        return std::make_optional<std::bitset<N>>(bitsString.value());
    }
};

template <typename T, std::size_t N>
class Serialiser<std::array<T, N>> {
public:
    static bool Validate(Context& context, const nlohmann::json& serialised)
    {
        return serialised.is_array() && serialised.size() == N && std::ranges::all_of(serialised, [&](const auto& value){ return Serialiser<T>::Validate(context, value); });
    }

    static void Serialise(DataWriter&& writer, const std::array<T, N>& range)
    {
        writer.SetFormatToArray(N);
        std::ranges::for_each(range, [&](const auto& value){ writer.PushBack(value); });
    }

    static std::optional<std::array<T, N>> Deserialise(DataReader&& reader)
    {
        auto count = reader.Size();
        if (!count) {
            return std::nullopt;
        }

        if (*count != N) {
            reader.LogError("Expected exactly " + std::to_string(N) + " items, but found " + std::to_string(*count) + ".");
            return std::nullopt;
        }

        std::array<T, N> deserialisedArray;
        for (std::size_t i; i < *count; ++i) {
            auto item = reader.PopBack<T>();
            if (item) {
                deserialisedArray[i] = *item;
            } else {
                return std::nullopt;
            }
        }
        return deserialisedArray;
    }
};

template <typename T>
class Serialiser<std::optional<T>> {
public:
    static bool Validate(Context& context, const nlohmann::json& serialised)
    {
        return (serialised == nullString) || Serialiser<T>::Validate(context, serialised);
    }

    static void Serialise(DataWriter&& writer, const std::optional<T>& toSerialise)
    {
        bool hasValue = toSerialise.has_value();

        writer.SetFormatToArray(hasValue ? 1 : 0);

        if (hasValue) {
            writer.Write(toSerialise.value());
        }
    }

    static std::optional<std::optional<T>> Deserialise(DataReader&& reader)
    {
        auto count = reader.Size();

        if (!count) {
            return std::nullopt;
        }

        switch (*count) {
        case 0:
            return std::make_optional(std::optional<T>{});
        case 1: {
            auto value = reader.PopBack<T>();
            if (!value) {
                return std::nullopt;
            }
            return value;
            }
            break;
        default:
            reader.LogError("Expected exactly 0 or 1 stored items, found " + std::to_string(*count) + ".");
            return std::nullopt;
        }
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
            return serialised.contains(key) && Serialiser<std::optional<Ts>>::Validate(context, serialised[key]);
        }());

        return valid;
    }

    static void Serialise(DataWriter&& writer, const std::variant<Ts...>& toSerialise)
    {
        writer.SetFormatToObject();

        // The following immediately called lambda is called once for each type
        ([&]()
        {
            if (std::holds_alternative<Ts>(toSerialise)) {
                writer.Insert(Key<Ts>(), std::make_optional<Ts>(std::get<Ts>(toSerialise)));
            } else {
                writer.Insert(Key<Ts>(), std::make_optional<Ts>(std::nullopt));
            }
        }(), ...);
    }

    static std::optional<std::variant<Ts...>> Deserialise(DataReader&& reader)
    {
        return Deserialise(reader, std::make_index_sequence<std::variant_size_v<std::variant<Ts...>>>{});
    }

private:
    template <typename T>
    static std::string Key()
    {
        return detail::TypeNameStr<T>();
    }

    template <std::size_t... Indices>
    static std::optional<std::variant<Ts...>> Deserialise(DataReader& reader, std::index_sequence<Indices...>)
    {
        std::variant<Ts...> ret;
        bool encounteredError = false;
        bool foundValue = false;
        ([&]()
        {
            if (encounteredError) {
                return;
            }

            auto storedOptional = reader.Extract<std::optional<Ts>>(Key<Ts>());
            if (!storedOptional) {
                reader.LogError("Expected an optional value to be stored for each type in the variant.");
                encounteredError = true;
            } else {
                std::optional<Ts> optionalValue = storedOptional.value();
                if (optionalValue) {
                    ret.emplace(*optionalValue);
                    foundValue = true;
                }
            }
        }(), ...);

        if (!foundValue) {
            reader.LogError("Expected an optional value to be stored for each type in the variant.");
            return std::nullopt;
        }

        if (encounteredError) {
            return std::nullopt;
        }

        return ret;
    }
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

    static void Serialise(DataWriter&& writer, const std::unique_ptr<T>& toSerialise)
    {
        writer.SetFormatToValue();
        if (toSerialise == nullptr) {
            writer.Write(nullPointerString);
        } else if constexpr (HasPolymorphismHelperSpecialisation<T>) {
            PolymorphismHelper<T>::Write(writer, *toSerialise.get());
        } else {
            writer.Write(*toSerialise);
        }
    }

    static std::optional<std::unique_ptr<T>> Deserialise(DataReader&& reader)
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

    static void Serialise(DataWriter&& writer, const std::shared_ptr<T>& toSerialise)
    {
        writer.SetFormatToObject();
        std::uintptr_t pointerValue = reinterpret_cast<std::uintptr_t>(toSerialise.get());
        writer.Insert(uniqueIdentifierKey, pointerValue);

        if (toSerialise == nullptr) {
            writer.Insert(wrappedTypeKey, nullPointerString);
        } else if constexpr (HasPolymorphismHelperSpecialisation<T>) {
            PolymorphismHelper<T>::Insert(writer, wrappedTypeKey, *toSerialise.get());
        } else {
            writer.Insert(wrappedTypeKey, *toSerialise);
        }
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

    static inline const std::string uniqueIdentifierKey = "ptr";
    static inline const std::string cacheName = "shared_ptr";
    static inline const std::string nullPointerString = "nullptr";

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
