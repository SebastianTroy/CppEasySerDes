#ifndef EASYSERDESPOLYMORPHISMHELPER_H
#define EASYSERDESPOLYMORPHISMHELPER_H

#include "Core.h"

#include <memory>

/**
 * This file provides a means to support polymorphism.
 *
 * The aim is to allow a single line definition of class relationships, so that
 * this library can handle polymorphic types.
 *
 * It is brittle in that you have to specify each type that can be supported at
 * compile time, rather than rely on new child types being picked up
 * automatically.
 */

namespace esd {

namespace internal {

    /**
     * Returns a nicer and more consistent name than typeid(T).name().
     */
    template <typename T>
    [[ nodiscard ]] constexpr std::string_view TypeName()
    {
        std::string_view name, prefix, suffix;
#ifdef __clang__
        name = __PRETTY_FUNCTION__;
        prefix = "std::string_view (anonymous namespace)::TypeName() [T = ";
        suffix = "]";
#elif defined(__GNUC__)
        name = __PRETTY_FUNCTION__;
        prefix = "constexpr std::string_view {anonymous}::TypeName() [with T = ";
        suffix = "; std::string_view = std::basic_string_view<char>]";
#elif defined(_MSC_VER)
        name = __FUNCSIG__;
        prefix = "class std::basic_string_view<char,struct std::char_traits<char> > __cdecl `anonymous-namespace'::TypeName<";
        suffix = ">(void)";
#endif
        name.remove_prefix(prefix.size());
        name.remove_suffix(suffix.size());
        return name;
    }

    template <typename T, typename...Ts>
    concept IsDerivedFromAtLeastOneOfExcludingSelf = (... || (!std::same_as<T, Ts> && std::derived_from<Ts, T>));

    template <typename...Ts>
    concept AllParentTypesHaveVirtualDestructor = (... && (!IsDerivedFromAtLeastOneOfExcludingSelf<Ts, Ts...> || std::has_virtual_destructor_v<Ts>));

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

} // end namespace internal

/**
 * This is the type that needs to be specialised by the user in order to support
 * a set of related types polymorphially.
 *
 * Note that if the base type is a valid value, it must be repeated in the
 * DerivedTypes parameter-pack
 */
template <typename T>
class PolymorphismHelper;

template <class T>
concept HasPolymorphismHelperSpecialisation = requires() { PolymorphismHelper<T>{}; } && std::same_as<T, typename PolymorphismHelper<T>::base_type>;

template <typename BaseType, typename... DerivedTypes>
requires std::has_virtual_destructor_v<BaseType>
      && (sizeof...(DerivedTypes) > 0)
      && internal::AllParentTypesHaveVirtualDestructor<DerivedTypes...>
      && (... && std::derived_from<DerivedTypes, BaseType>)
      && (... && TypeSupportedByEasySerDes<DerivedTypes>)
      && internal::AllTypesAreUnique<DerivedTypes...> // Not required but might help catch bugs
class PolymorphicSet {
public:
    // Used to sanity check that the T in PolymorphismHelper<T> matches BaseType
    using base_type = BaseType;

    /**
     * Returns `true` if serialised contains `typeNameKey`.
     */
    static bool ContainsPolymorphicType(const nlohmann::json& serialised)
    {
        return serialised.contains(typeNameKey_);
    }

    /**
     * Works out which of the DerivedTypes was serialised and calls
     * `esd::Validate<DerivedType>`.
     */
    static bool ValidatePolymorphic(const nlohmann::json& serialised)
    {
        bool valid = serialised.contains(typeNameKey_);
        if (valid) {
            std::string storedTypeName = serialised.at(typeNameKey_);
            auto copy = serialised;
            copy.erase(typeNameKey_);
            valid = ((storedTypeName == internal::TypeName<DerivedTypes>() && Validate<DerivedTypes>(copy)) || ...);
        }
        return valid;
    }

    /**
     * Works out which of the DerivedTypes `value` is an instance of and calls
     * `esd::Serialise<DerivedType>`.
     */
    static nlohmann::json SerialisePolymorphic(const BaseType& value)
    {
        nlohmann::json serialised;

        /*
         * The following uses an immediately called lambda as a container for
         * multiple statements that need to be parameter un-packed together.
         *
         * The aim of this code is simply to find the most derived type that the
         * input can be sucessfully dynamically cast to, so that when it is
         * serialised, no information is lost.
         *
         * The immediately called lambda returns a bool and the unpack chains
         * "logical or" so it "quits early" once it has found the correct type.
         */
        ([&]() -> bool
        {
            // This will be a single type due to parameter pack expansion
            using CurrentType = DerivedTypes;

            if (typeid(CurrentType).name() == typeid(value).name()) {
                serialised = Serialise(dynamic_cast<const CurrentType&>(value));
                serialised[typeNameKey_] = internal::TypeName<CurrentType>();
                return true; // Stop checking subsequent types
            }
            return false; // Continue checking the other types
        }() || ...);

        return serialised;
    }

    /**
     * Works out which of the DerivedTypes was serialised and calls
     * `esd::Deserialise<std::unique_ptr<DerivedType>>`.
     */
    static std::unique_ptr<BaseType> DeserialisePolymorphic(const nlohmann::json& serialised)
    {
        std::unique_ptr<BaseType> deserialised = nullptr;

        if (serialised.contains(typeNameKey_)) {
            std::string storedTypeName = serialised.at(typeNameKey_);
            auto copy = serialised;
            copy.erase(typeNameKey_);

            /*
             * The following uses an immediately called lambda as a container
             * for multiple statements that need to be parameter un-packed
             * together.
             *
             * The aim of this code is simply to check each type's name against
             * the stored typeName, if it finds the type, it'll deserialise it.
             * If the type is supported via the ClassHelper, it will deserialise
             * the type in-place, avoiding any moves or copies, otherwise it
             * deserialises it normally.
             *
             * The immediately called lambda returns a bool and the unpack
             * chains "logical or" so it "quits early" once it has found the
             * correct type.
             */
            ([&]() -> bool
            {
                // This will be a single type due to parameter pack expansion
                using CurrentType = DerivedTypes;

                if (storedTypeName == internal::TypeName<CurrentType>()) {
                    deserialised = DeserialiseWithoutChecks<std::unique_ptr<CurrentType>>(copy);
                    return true;
                }
                return false;
            }() || ...);
        }

        return deserialised;
    }

private:
    static const inline std::string typeNameKey_ = "__typeName";
};

} // end namespace esd

#endif // EASYSERDESPOLYMORPHISMHELPER_H
