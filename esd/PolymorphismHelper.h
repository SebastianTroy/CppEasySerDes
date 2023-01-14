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

namespace detail {

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

    template <typename T>
    struct DummyValueForPolymorphicWriting {
        const std::string& typenameKey_;
        std::string typename_;
        const std::string& valueKey_;
        const T& value_;
    };

} // end namespace detail

template<typename T>
class Serialiser<detail::DummyValueForPolymorphicWriting<T>> {
public:
    static bool Validate(Context& context, const nlohmann::json& dataSource)
    {
        assert(false && "Designed to work with serialisation ONLY");
        return false;
    }

    static void Serialise(DataWriter&& writer, const detail::DummyValueForPolymorphicWriting<T>& dummy)
    {
        writer.SetFormatToObject();
        writer.Insert(dummy.typenameKey_, dummy.typename_);
        writer.Insert(dummy.valueKey_, dynamic_cast<const T&>(dummy.value_));
    }

    T Deserialise(Context& context, const nlohmann::json& dataSource)
    {
        assert(false && "Designed to work with serialisation ONLY");
        return { "", *reinterpret_cast<const T*>(nullptr) };
    }
};

/**
 * This is the type that needs to be specialised by the user in order to support
 * a set of related types polymorphially.
 *
 * It is intended that the user extends esd::PolymorphicSet
 */
template <typename T>
class PolymorphismHelper;

template <class T>
concept HasPolymorphismHelperSpecialisation = requires() { PolymorphismHelper<T>{}; } && std::same_as<T, typename PolymorphismHelper<T>::base_type>;

/**
 * Note that if the base type is not abstract, it must be repeated in the
 * DerivedTypes parameter-pack to be considered as an option.
 */
template <typename BaseType, typename... DerivedTypes>
requires std::has_virtual_destructor_v<BaseType>
      && (sizeof...(DerivedTypes) > 0)
      && (... && !std::is_abstract_v<DerivedTypes>)
      && detail::AllParentTypesHaveVirtualDestructor<DerivedTypes...>
      && (... && std::derived_from<DerivedTypes, BaseType>)
      && (... && TypeSupportedByEasySerDes<DerivedTypes>)
      && detail::AllTypesAreUnique<DerivedTypes...> // Not required but might help catch bugs
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
    static bool ValidatePolymorphic(Context& context, const nlohmann::json& serialised)
    {
        bool valid = serialised.contains(typeNameKey_) && serialised.contains(valueKey_);
        if (valid) {
            std::string storedTypeName = serialised.at(typeNameKey_);
            valid = ((storedTypeName == detail::TypeName<DerivedTypes>() && Validate<DerivedTypes>(context, serialised.at(valueKey_))) || ...);
        }
        return valid;
    }

    /**
     * Works out which of the DerivedTypes `value` is an instance of and calls
     * `writer.Write(value)`.
     */
    static void Write(DataWriter& writer, const BaseType& value)
    {
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
        bool success = ([&]() -> bool
        {
            // This will be a single type due to parameter pack expansion
            using CurrentType = DerivedTypes;

            // TODO look into how to achieve this when RTTI is switched off
            if (typeid(CurrentType).name() == typeid(value).name()) {
                writer.Write(detail::DummyValueForPolymorphicWriting<CurrentType>{ typeNameKey_, detail::TypeNameStr<CurrentType>(), valueKey_, dynamic_cast<const CurrentType&>(value) });
                return true; // Stop checking subsequent types
            }
            return false; // Continue checking the other types
        }() || ...);

        if (!success) {
            writer.LogError("PolymorphismHelper::Write Unsupported derived type detected for value, expected " + SupportedTypesStr() + ", recieved " + typeid(value).name() + ".");
        }
    }

    /**
     * Works out which of the DerivedTypes `value` is an instance of and calls
     * `writer.PushBack(value)`.
     */
    static void PushBack(DataWriter& writer, const BaseType& value)
    {
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
        bool success = ([&]() -> bool
        {
            // This will be a single type due to parameter pack expansion
            using CurrentType = DerivedTypes;

            // TODO look into how to achieve this when RTTI is switched off
            if (typeid(CurrentType).name() == typeid(value).name()) {
                writer.PushBack(detail::DummyValueForPolymorphicWriting<CurrentType>{ typeNameKey_, detail::TypeNameStr<CurrentType>(), valueKey_, dynamic_cast<const CurrentType&>(value) });
                return true; // Stop checking subsequent types
            }
            return false; // Continue checking the other types
        }() || ...);

        if (!success) {
            writer.LogError("PolymorphismHelper::PushBack Unsupported derived type detected for item, expected " + SupportedTypesStr() + ", recieved " + typeid(value).name() + ".");
        }
    }

    /**
     * Works out which of the DerivedTypes `value` is an instance of and calls
     * `writer.Insert(label, value)`.
     */
    static void Insert(DataWriter& writer, const std::string& label, const BaseType& value)
    {
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
        bool success = ([&]() -> bool
        {
            // This will be a single type due to parameter pack expansion
            using CurrentType = DerivedTypes;

            // TODO look into how to achieve this when RTTI is switched off
            if (typeid(CurrentType).name() == typeid(value).name()) {
                writer.Insert(label, detail::DummyValueForPolymorphicWriting<CurrentType>{ typeNameKey_, detail::TypeNameStr<CurrentType>(), valueKey_, dynamic_cast<const CurrentType&>(value) });
                return true; // Stop checking subsequent types
            }
            return false; // Continue checking the other types
        }() || ...);

        if (!success) {
            writer.LogError("PolymorphismHelper::Insert Unsupported derived type detected for value with label '" + label + "', expected " + SupportedTypesStr() + ", recieved " + typeid(value).name() + ".");
        }
    }

    /**
     * Works out which of the DerivedTypes was serialised and calls
     * `esd::Deserialise<std::unique_ptr<DerivedType>>`.
     *
     * FIXME use the ClassHelper DeserialiseInPlace semantics to allow return types other than unique_ptr
     */
    static std::unique_ptr<BaseType> DeserialisePolymorphic(Context& context, const nlohmann::json& serialised)
    {
        std::unique_ptr<BaseType> deserialised = nullptr;

        if (serialised.contains(typeNameKey_)) {
            std::string storedTypeName = serialised.at(typeNameKey_);

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

                if (storedTypeName == detail::TypeName<CurrentType>()) {
                    deserialised = DeserialiseWithoutChecks<std::unique_ptr<CurrentType>>(context, serialised.at(valueKey_));
                    return true;
                }
                return false;
            }() || ...);
        }

        return deserialised;
    }

private:
    static const inline std::string typeNameKey_ = "typeName";
    static const inline std::string valueKey_ = "instance";

    static std::string SupportedTypesStr()
    {
        std::stringstream typesStr;

        typesStr << "{ [" << detail::TypeNameStr<BaseType>() << "]";

        ((typesStr << ", " << detail::TypeNameStr<DerivedTypes>()), ...);

        typesStr << " }";

        return typesStr.str();
    }
};

} // end namespace esd

#endif // EASYSERDESPOLYMORPHISMHELPER_H
