#ifndef EASYSERDESPOLYMORPHICCLASSHELPER_H
#define EASYSERDESPOLYMORPHICCLASSHELPER_H

#include "EasySerDesClassHelper.h"

#include <memory>

namespace esd {
namespace {
    template <typename T>
    [[ nodiscard ]] constexpr std::string_view TypeName()
    {
        std::string_view name, prefix, suffix;
    #ifdef __clang__
        name = __PRETTY_FUNCTION__;
        prefix = "std::string_view esd::(anonymous namespace)::TypeName() [T = ";
        suffix = "]";
    #elif defined(__GNUC__)
        name = __PRETTY_FUNCTION__;
        prefix = "constexpr std::string_view esd::{anonymous}::TypeName() [with T = ";
        suffix = "; std::string_view = std::basic_string_view<char>]";
    #elif defined(_MSC_VER)
        name = __FUNCSIG__;
        prefix = "class std::basic_string_view<char,struct std::char_traits<char> > __cdecl esd::`anonymous-namespace'::TypeName<";
        suffix = ">(void)";
    #endif
        name.remove_prefix(prefix.size());
        name.remove_suffix(suffix.size());
        return name;
    }

    // https://stackoverflow.com/questions/70130735/c-concept-to-check-for-derived-from-template-specialization
    template <template <class...> class Z, class... Args>
    void is_derived_from_specialization_of(const Z<Args...>&);

    template <typename T, typename... Ts>
    concept NotDerivedFromAnyOf = (... && !(std::derived_from<T, Ts> && !std::same_as<T, Ts>));

    template <typename T, typename...Ts>
    constexpr bool AllTypesAreUniqueRecursive()
    {
        if constexpr (sizeof...(Ts) > 0) {
            return (... && !std::same_as<T, Ts>) && AllTypesAreUniqueRecursive<Ts...>();
        }
        return true;
    }

    template <typename...Ts>
    concept AllTypesAreUnique = AllTypesAreUniqueRecursive<Ts...>();

} // end anon namespace

template <class T, template <class...> class Z>
concept IsDerivedFromSpecialisationOf = requires(const T& t) {
    is_derived_from_specialization_of<Z>(t);
};

template <typename T, typename... ConstructionArgs>
class JsonPolymorphicClassSerialiser : public JsonClassSerialiser<T, ConstructionArgs...> {
public:

    // Validate|Serialise|DeserialisePolymorphic each check all of their child types to see if a child method ought to be called instead
    // if not they assume T is the correct type forward the call onto the appropriate parent Validate|Serialise|Deserialise call

    static bool ValidatePolymorphic(const nlohmann::json& serialised)
    {
        if (serialised.contains(typeNameKey_)) {
            std::string serialisedTypeName = serialised.at(typeNameKey_);

            if (typeName_ == serialisedTypeName) {
                nlohmann::json copy = serialised;
                copy.erase(typeNameKey_);
                return JsonClassSerialiser<T, ConstructionArgs...>::Validate(copy);
            } else {
                for (const ChildTypeHelper& h : childHelpers_) {
                    if (h.recursiveTypeNameChecker_(serialisedTypeName)) {
                        return h.validator_(serialised);
                    }
                }
            }
        }
        return false;
    }

    static nlohmann::json SerialisePolymorphic(const T& value)
    {
        for (const ChildTypeHelper& h : childHelpers_) {
            if (h.isInstanceChecker_(value)) {
                return h.serialiser_(value);
            }
        }

        nlohmann::json serialised = JsonClassSerialiser<T, ConstructionArgs...>::Serialise(value);
        serialised[typeNameKey_] = typeName_;
        return serialised;
    }

    static std::unique_ptr<T> DeserialisePolymorphic(const nlohmann::json& serialised)
    {
        if (serialised.contains(typeNameKey_)) {
            std::string serialisedTypeName = serialised.at(typeNameKey_);

            if (typeName_ == serialisedTypeName) {
                nlohmann::json copy = serialised;
                copy.erase(typeNameKey_);

                auto makeUniquePtr = &JsonSerialiser<T>::template ConstructionArgsForwarder<MakeUniqueWrapper, T>::Invoke;
                return JsonClassSerialiser<T, ConstructionArgs...>::Deserialise(makeUniquePtr, copy);
            } else {
                for (const ChildTypeHelper& h : childHelpers_) {
                    if (h.recursiveTypeNameChecker_(serialisedTypeName)) {
                        return h.deserialiser_(serialised);
                    }
                }
            }
        }
        return nullptr;
    }

    static bool RecursivelyCheckTypeName(const std::string& typeName)
    {
        return typeName == typeName_ || std::ranges::any_of(childHelpers_, [&](const ChildTypeHelper& h)
                                                                           {
                                                                               return h.recursiveTypeNameChecker_(typeName);
                                                                           });
    }

    template <typename... ChildTypes>
    requires (sizeof...(ChildTypes) > 0)
          && AllTypesAreUnique<ChildTypes...>
          && (... && std::derived_from<ChildTypes, T>)
          && (... && NotDerivedFromAnyOf<ChildTypes, ChildTypes...>)
          && (... && TypeSupportedByEasySerDes<ChildTypes>)
          && (... && IsDerivedFromSpecialisationOf<JsonSerialiser<ChildTypes>, esd::JsonPolymorphicClassSerialiser>)
    static void RegisterChildTypes()
    {
        childHelpers_.clear();
        (..., RegisterChild<ChildTypes>());
    }

private:
    struct ChildTypeHelper {
        std::string typeName_;
        std::function<bool(const nlohmann::json& serialised)> validator_ = nullptr;
        std::function<nlohmann::json(const T& toSerialise)> serialiser_ = nullptr;
        std::function<std::unique_ptr<T>(const nlohmann::json& serialised)> deserialiser_ = nullptr;

        std::function<bool(const T& instance)> isInstanceChecker_ = nullptr;
        std::function<bool(const std::string& typeName)> recursiveTypeNameChecker_ = nullptr;
    };

    template <typename Type, typename... Params>
    struct MakeUniqueWrapper {
        static std::unique_ptr<Type> Invoke(Params... args)
        {
            return std::make_unique<Type>(std::forward<Params>(args)...);
        }
    };

    static const inline std::string typeNameKey_ = "__typeName";

    static inline std::string typeName_ = std::string(TypeName<T>());
    static inline std::vector<ChildTypeHelper> childHelpers_ = {};

    template <typename ChildType>
    static void RegisterChild()
    {
        std::string childTypeName{ TypeName<ChildType>() };

        childHelpers_.emplace_back(
            childTypeName,
            [](const nlohmann::json& serialised) -> bool
            {
                return JsonSerialiser<ChildType>::ValidatePolymorphic(serialised);
            },
            [=](const T& toSerialise) -> nlohmann::json
            {
                return JsonSerialiser<ChildType>::SerialisePolymorphic(dynamic_cast<const ChildType&>(toSerialise));
            },
            [](const nlohmann::json& serialised) -> std::unique_ptr<T>
            {
                return JsonSerialiser<ChildType>::DeserialisePolymorphic(serialised);
            },
            [](const T& instance) -> bool
            {
                return dynamic_cast<const ChildType*>(&instance) != nullptr;
            },
            [](const std::string& typeName) -> bool
            {
                return JsonSerialiser<ChildType>::RecursivelyCheckTypeName(typeName);
            }
        );
    }
};

} // end namespace esd
#endif // EASYSERDESPOLYMORPHICCLASSHELPER_H
