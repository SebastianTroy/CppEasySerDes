#ifndef EASYSERDESPOLYMORPHISMHELPER_H
#define EASYSERDESPOLYMORPHISMHELPER_H

#include "Core.h"

#include <memory>

namespace {

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
    constexpr bool AllTypesAreUniqueRecursive()
    {
        if constexpr (sizeof...(Ts) > 0) {
            return (... && !std::same_as<T, Ts>) && AllTypesAreUniqueRecursive<Ts...>();
        }
        return true;
    }

    template <typename...Ts>
    concept AllTypesAreUnique = AllTypesAreUniqueRecursive<Ts...>();

} // end anonymous namespace

namespace esd {

/**
 * This is the type that needs to be specialised by the user in order to support
 * a set of related types polymorphially.
 *
 * Note that if the base type is a valid value, it must be repeated in the
 * DerivedTypes parameter-pack
 *                                                                                v (only required if BaseType isn't pure virtual)
 * class esd::PolymorphismHelper<BaseType> : public esd::PolymorphicSet<BaseType, BaseType, Child1, Child2, GrandChild, GreatGrandChild...>{}
 */
template <typename T>
class PolymorphismHelper;

template <class T>
concept HasPolymorphismHelperSpecialisation = requires() { PolymorphismHelper<T>{}; } && std::same_as<T, typename PolymorphismHelper<T>::base_type>;

template <typename BaseType, typename... DerivedTypes>
requires (sizeof...(DerivedTypes) > 0)
      && (... && std::derived_from<DerivedTypes, BaseType>)
      && (... && TypeSupportedByEasySerDes<DerivedTypes>)
      && AllTypesAreUnique<DerivedTypes...> // Not required but might help catch bugs
class PolymorphicSet {
public:
    // Used to sanity check that the T in PolymorphismHelper<T> matches BaseType
    using base_type = BaseType;

    /**
     * Returns false if `value` is an instance of BaseType, `true` if it is any
     * of the other `DerivedTypes`.
     */
    template <typename CandidateType>
    requires (... || std::same_as<CandidateType, DerivedTypes>)
    static bool IsDerivedType(const CandidateType& value)
    {
        return IsAnotherTypeBetterMatched<BaseType>(value);
    }

    /**
     * Returns `true` if serialised contains `typeNameKey`.
     *
     * You might expect this to return based on whether the typeName stored in
     * the json matched `TypeName<BaseType>()`, but `SerialisePolymorphic`
     * actually strips the `typeNameKey` out of the JSON so must always be
     * called  before `Serialise<CandidateType>` can be called succesfully.
     *
     * Again this stops infinitely recursive calls between `Serialise<T>` and
     * `PolymorphismHeler<BaseType>::SerialisePolymorphic` (and for Validate).
     */
    static bool IsDerivedType(const nlohmann::json& serialised)
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
            valid = ((storedTypeName == TypeName<DerivedTypes>() && Validate<DerivedTypes>(copy)) || ...);
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

            bool typeMatched = dynamic_cast<const CurrentType*>(&value);
            if (typeMatched) {
                // Only call esd::Serialise with the most derived type possible
                if (!IsAnotherTypeBetterMatched<CurrentType>(value)) {
                    serialised = Serialise(dynamic_cast<const CurrentType&>(value));
                    serialised[typeNameKey_] = TypeName<CurrentType>();
                    return true; // Stop checking subsequent types
                }
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

                if (storedTypeName == TypeName<CurrentType>()) {
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

    template <typename CandidateType, typename RefType>
    static bool IsAnotherTypeBetterMatched(const RefType& ref)
    {
        // Don't re-check the current type
        // && The other type is also a potential match
        // && The other type is more derived than CurrentType and therefore is a better match
        // If this is true for any type, return true
        return ((!std::same_as<CandidateType, DerivedTypes>
                && dynamic_cast<const DerivedTypes*>(&ref)
                && std::is_base_of_v<CandidateType, DerivedTypes>) || ...);
    }

};

} // end namespace esd

#endif // EASYSERDESPOLYMORPHISMHELPER_H
