#ifndef EASYSERDESCLASSHELPER_H
#define EASYSERDESCLASSHELPER_H

#include "Core.h"

#include <nlohmann/json.hpp>

#include <type_traits>
#include <assert.h>

/**
 * This file allows users to more easily support their own class types.
 *
 * The entire aim of ClassHelper is to prevent the need for a user to define
 * seperate Validate, Serialise, and Deserialise functions when creating an
 * esd::Serialiser specialisation for their own classes/structs.
 */

namespace esd {

namespace internal {
    /**
     * This concept is used in cases where a factory produces e.g. a shared
     * pointer and we then dereference the output to complete deserialisation.
     */
    template <typename T, typename BoxType>
    concept TypeIsDereferencableFrom = requires (BoxType b)
    {
        { *b } -> std::same_as<T&>;
    };

    /**
     * Reduces code bloat without compromising readability
     */
    template <typename F, typename... Args>
    using ReturnTypeNoCVRef = std::remove_cvref_t<std::invoke_result_t<F, Args...>>;

    /**
     * Forward declaration of helper type, because it was desirable to define
     * ClassHelper first. This helper encapsulates implementation details, and
     * also makes it easy to call Configure once.
     */
    template <typename T, typename... ConstructionArgs>
    requires std::is_class_v<T>
    class Implementation;

    /**
     * Used to make sure the user isn't trying to read/write to/from class
     * members that are pointers or references.
     */
    template <typename T>
    concept IsPointerOrReference = std::is_pointer_v<T> || std::is_reference_v<T>;

    /**
     * Used to make sure a factory succeeded, and that we aren't about to try
     * and dereference a nullptr or box type containing a nullptr.
     */
    template <typename T>
    concept IsComparableToNullptr = requires (T t) { { t == nullptr } -> std::same_as<bool>; };

    /**
     * This struct holds type information to assist template type typededuction
     * in various places. It is used when defining how the library will call a
     * constructor or a function during deserialisation.
     */
    template <typename ParamType>
    struct Parameter {
    public:
        using value_t = ParamType;
        std::string parameterKey_;
    };

} // end namespace internal

/**
 * @brief The ClassHelper class should be extended by the user in order to allow
 * CppEasySerDes to support custom user types:
 * `esd::Serialiser<T> : public esd::ClassHelper<T, Args...>`
 *
 * T                Is the type to be supported.
 *
 * ConstructionArgs Are the parameters required to create an instance of the
 *                  type, via one of brace initialisation, a constructor, or a
 *                  factory function call.
 *
 * This implementation supplies the Validate, Serialise, and Deserialise
 * functions for the user. It achieves this by getting the user to register the
 * set of values required to losslesly store it. It is important to be flexible
 * in how these values are gained during serialisation, and also in how they are
 * applied to the type during deserialisation.
 *
 * To support constructors and function calls during deserialisation, some of
 * the values will be registered as parameters to those same calls. Values not
 * used in construction or initialisation calls can be registered seperately.
 *
 * There are also Deserialise overloads to support building the type "in place",
 * for example with std::make_shared, std::make_unique, emplace_back etc. as
 * targets.
 */
template <typename T, typename... ConstructionArgs>
requires std::is_class_v<T>
      && std::is_destructible_v<T>
      && (!std::is_abstract_v<T>)
      && (!(... || internal::IsPointerOrReference<ConstructionArgs>))
class ClassHelper {
public:
    template <typename ParamT>
    using Parameter = internal::Parameter<ParamT>;

    template <typename Invocable, typename... InvocableArgs>
    using ReturnTypeNoCVRef = internal::ReturnTypeNoCVRef<Invocable, InvocableArgs...>;

    ///
    /// SetConstruction
    /// ---------------
    ///
    /// The intent here was to allow the user to easily and clearly state how
    /// their type should be constructed.
    ///
    /// Ultimately, the constructor only needs to know which stored values to
    /// use, and in which order. It could have been that the user specified all
    /// of the values required to support their type, and then seperately
    /// specified which of those values were needed by the constructor. This
    /// felt like a bad option, because under the hood parameter and
    /// non-parameter values need to be treated subtly differently.
    ///
    /// Instead I have opted to combine the steps of registering the values, and
    /// defining the construction. Parameter<> is a templated type, and is not
    /// visible to the user, meaning that they have to create a Parameter using
    /// this API, and they have to do it inside a call to SetConstruction (or
    /// AddInitialisationCall).
    ///

    /**
     * This overload simply takes a sequence of parameters that can be created
     * by calling CreateParameter(...). This will either call a constructor, or
     * brace initialise the type.
     */
    static void SetConstruction(Parameter<std::remove_cvref_t<ConstructionArgs>>... params)
    {
        helper_.SetConstruction(std::move(params)...);
    }

    /**
     * This overload takes a sequence of parameters that can be created by
     * calling CreateParameter(...). This will either call a constructor, or
     * brace initialise the type.
     *
     * It also takes a validator, which must be a callable with the same
     * function signature as the constructor (or the brace initialiser), and
     * returns a boolean.
     * While each parameter will have its own individual validation, sometimes
     * the validity of a parameter can depend on the value of another.
     * This validator can be used to check the values with relation to each
     * other, so that a call to Validate can indicate if construction of the
     * type might fail.
     *
     * FIXME this needs testing
     */
    template <typename Validator>
    requires std::is_invocable_r_v<bool, Validator, const std::remove_cvref_t<ConstructionArgs>&...>
    static void SetConstruction(Parameter<std::remove_cvref_t<ConstructionArgs>>... params, Validator parameterValidator)
    {
        helper_.SetConstruction(std::move(params)..., std::forward<Validator>(parameterValidator));
    }

    /**
     * This overload takes a sequence of parameters that can be created by
     * calling CreateParameter(...). These parameters will be used to call the
     * specified factory function.
     *
     * The specified factory, must be a callable that returns an instance of
     * the type supported by this class.
     *
     * FIXME this needs testing
     */
    template <typename... FactoryArgs, typename Factory>
    requires std::is_invocable_r_v<T, Factory, FactoryArgs...>
    static void SetConstruction(Factory factory, Parameter<std::remove_cvref_t<FactoryArgs>>... params)
    {
        helper_SetConstruction(std::forward<Factory>(factory), std::move(params)...);
    }

    /**
     * This overload takes a sequence of parameters that can be created by
     * calling CreateParameter(...). These parameters will be used to call the
     * specified factory function.
     *
     * The specified factory, must be a callable that returns an instance of
     * the type supported by this class.
     *
     * It also takes a validator, which must be a callable with the same
     * function signature as the constructor (or the brace initialiser), and
     * returns a boolean.
     * While each parameter will have its own individual validation, sometimes
     * the validity of a parameter can depend on the value of another.
     * This validator can be used to check the values with relation to each
     * other, so that a call to Validate can indicate if construction of the
     * type might fail.
     *
     * FIXME this needs testing
     */
    template <typename... FactoryArgs, typename Factory, typename Validator>
    requires std::is_invocable_r_v<bool, Validator, const FactoryArgs&...>
          && std::is_invocable_r_v<T, Factory, FactoryArgs...>
    static void SetConstruction(Factory factory, Parameter<std::remove_cvref_t<FactoryArgs>>... params, Validator parameterValidator)
    {
        helper_.SetConstruction(std::forward<Factory>(factory), std::move(params)..., std::forward<Validator>(parameterValidator));
    }

    ///
    /// AddInitialisationCall
    /// ---------------------
    ///
    /// The intent here was to allow the user to easily and clearly state how
    /// their type's state should be initialised, via one or more calls to
    /// member functions after an instance is constructed.
    ///
    /// Ultimately, each call only needs to know which stored values to use, and
    /// in which order. It could have been that the user specified all of the
    /// values required to support their type, and then seperately specified
    /// which of those values were needed by each additional initialisation
    /// call. This felt like a bad option, because under the hood parameter and
    /// non-parameter values need to be treated subtly differently.
    ///
    /// Instead I have opted to combine the steps of registering the values, and
    /// defining the function calls. Parameter<> is a templated type, and is not
    /// visible to the user, meaning that they have to create a Parameter using
    /// this API, and they have to do it inside a call to AddInitialisationCall
    /// (or SetConstruction).
    ///

    /**
     * This overload takes an object member function pointer, and a sequence of
     * zero or more parameters that can be created by calling
     * CreateParameter(...).
     *
     * This will call the specified member function after an instance has been
     * constructed during deserialisation.
     */
    template <typename MemberFunctionPointer, typename... Ts>
    requires std::is_member_function_pointer_v<MemberFunctionPointer>
          && std::is_invocable_v<MemberFunctionPointer, T&, Ts...>
    static void AddInitialisationCall(MemberFunctionPointer initialisationCall, Parameter<Ts>... params)
    {
        helper_.AddInitialisationCall(initialisationCall, std::move(params)...);
    }


    /**
     * This overload takes a pointer to a member function, and a sequence of
     * zero or more parameters that can be created by calling
     * CreateParameter(...).
     *
     * This will call the specified member function after an instance has been
     * constructed during deserialisation.
     *
     * It also takes a validator, which must be a callable with the same
     * parameters as the object member function pointer, returning a bool.
     * While each parameter will have its own individual validation, sometimes
     * the validity of a parameter can depend on the value of another.
     * This validator can be used to check the values with relation to each
     * other, so that a call to Validate can indicate if initialisation of the
     * type might fail.
     *
     * This validation only makes sense when there is more than one parameter.
     */
    template <typename Validator, typename MemberFunctionPointer, typename... Ts>
    requires std::is_member_function_pointer_v<MemberFunctionPointer>
          && std::is_invocable_v<MemberFunctionPointer, T&, Ts...>
          && std::is_invocable_r_v<bool, Validator, const Ts&...>
          && (sizeof...(Ts) > 1)
    static void AddInitialisationCall(Validator parameterValidator, MemberFunctionPointer initialisationCall, Parameter<Ts>... params)
    {
        helper_.AddInitialisationCall(std::forward<Validator>(parameterValidator), initialisationCall, std::move(params)...);
    }

    ///
    /// CreateParameter
    /// ---------------------
    ///
    /// The intent here was to bridge the gap between telling EasySerDes which
    /// values need to be stored for a given type, and how to use them during
    /// deserialisation. Ideally in a way that preserves type information so
    /// that users don't need to explicitly define the template parameters for
    /// their calls to SetConstruction and AddInitialisationCall.
    ///
    /// To achieve this CreateParameter is similar to RegisterVariable, except
    /// it only needs to be told how to get the value during serialisation, this
    /// is because we can be guaranteed that the user can only use
    /// CreateParameter when defining the construction or initialisation call
    /// that will use the value.
    ///
    /// It is also paired with the Parameter<> struct, which simply contains
    /// the key for the registered value, and preserves the type of the value so
    /// calls to SetConstruction and Register initialisation can be type
    /// checked, and simple, i.e. they basically take a sequence of strings
    /// instructing them on which values to use for their calls.
    ///

    /**
     * This overload is for member pointers, and function pointers/lambdas that
     * take a single "const T&" parameter.
     *
     * Optionally a label can be set, if it is not set then a unique label will
     * be generated automatically.
     *
     * Optionally additional validation can be added. This does not remove the
     * default validation.
     */
    template <class Invocable>
    requires std::is_invocable_v<Invocable, const T&>
    [[ nodiscard ]] static Parameter<ReturnTypeNoCVRef<Invocable, const T&>> CreateParameter(Invocable&& valueGetter, std::optional<std::string>&& label = std::nullopt, std::optional<std::function<bool(const ReturnTypeNoCVRef<Invocable, const T&>&)>>&& customValidator = std::nullopt)
    {
        return helper_.CreateParameter(std::forward<Invocable>(valueGetter), std::move(label), std::move(customValidator));
    }

    /**
     * This overload is for function pointers/lambdas that accept no parameters.
     *
     * Optionally a label can be set, if it is not set then a unique label will
     * be generated automatically.
     *
     * Optionally additional validation can be added. This does not remove the
     * default validation.
     */
    template <class Invocable>
    requires std::is_invocable_v<Invocable>
    [[ nodiscard ]] static Parameter<ReturnTypeNoCVRef<Invocable>> CreateParameter(Invocable&& valueGetter, std::optional<std::string>&& label = std::nullopt, std::optional<std::function<bool(const ReturnTypeNoCVRef<Invocable>&)>>&& customValidator = std::nullopt)
    {
        return helper_.CreateParameter(std::forward<Invocable>(valueGetter), std::move(label), std::move(customValidator));
    }

    /**
     * This overload is for "magic values".
     *
     * Optionally a label can be set, if it is not set then a unique label will
     * be generated automatically.
     *
     * Additional validation is automatically added to ensure the stored value
     * matches the magic value.
     * TODO make it possilke to opt-out of this additional validation.
     */
    template <typename ParamType>
    [[ nodiscard ]] static Parameter<ParamType> CreateParameter(const ParamType& constValue, std::optional<std::string>&& label = std::nullopt)
    {
        helper_.CreateParameter(constValue, std::move(label));
    }

    ///
    /// RegisterVariable
    /// ----------------
    ///
    /// The intent here is to allow the user to define values that are needed to
    /// serialise/deserialise their type, but which aren't part of any
    /// construction or initialisation calls.
    ///
    /// Conceptually any user type is a list of values that are needed to
    /// save/load the type losslesly. So theoretically the user would call this
    /// function for each item that was in that list, and then if applicable,
    /// also say which of those items were needed for construction or
    /// initialisation.
    ///
    /// Technically POD type structs can be supported entirely via this
    /// function, without ever calling SetConstruction. (though there are
    /// benefits to calling SetConstruction instead).
    ///
    /// Despite this being the first piece of the puzzle used to create this
    /// library in the first place, its usage should actually be quite rare.
    ///

    /**
     * This overload is for public object member pointers.
     *
     * This call uses the same object member pointer to access the value for
     * both serialisation and deserialisation of the type.
     *
     * Optionally a label can be set, if it is not set then a unique label will
     * be generated automatically.
     *
     * Optionally additional validation can be added. This does not remove the
     * default validation.
     */
    template <class MemberObjectPointer>
    requires std::is_member_object_pointer_v<MemberObjectPointer>
          && std::is_invocable_v<MemberObjectPointer, T&>
    static void RegisterVariable(MemberObjectPointer getterAndSetter, std::optional<std::string>&& label = std::nullopt, std::optional<std::function<bool(const ReturnTypeNoCVRef<MemberObjectPointer, T&>&)>>&& customValidator = std::nullopt)
    {
        helper_.RegisterVariable(getterAndSetter, std::move(label), std::move(customValidator));
    }

    /**
     * This overload is for public member object function pointers, or
     * lambdas/functions where the getter's signature must accept a single
     * `(const T&)` parameter, and return a `ValueType`, and the setter's
     * signature must accept `(T&, const ValueType&)` parameters, where
     * ValueType is the same type returned by the getter.
     *
     * This call uses a seperate getter and setter for serialisation and
     * deserialisation respectively. This is necessary when the value is not a
     * public member of the type.
     *
     * Optionally a label can be set, if it is not set then a unique label will
     * be generated automatically.
     *
     * Optionally additional validation can be added. This does not remove the
     * default validation.
     */
    template <typename Getter, typename Setter>
    requires std::is_invocable_v<Getter, const T&>
          && std::is_invocable_v<Setter, T&, ReturnTypeNoCVRef<Getter, const T&>>
    static void RegisterVariable(Getter&& getter, Setter&& setter, std::optional<std::string>&& label = std::nullopt, std::optional<std::function<bool(const ReturnTypeNoCVRef<Getter, const T&>&)>>&& customValidator = std::nullopt)
    {
        helper_.RegisterVariable(std::forward<Getter>(getter), std::forward<Setter>(setter), std::move(label), std::move(customValidator));
    }

    /**
     * This overload is for public member object function pointers, or
     * lambdas/functions where the getter's signature must accept no parameter,
     * and return a `ValueType`, and the setter's signature must accept
     * `(T&, const ValueType&)` parameters, where ValueType is the same type
     * returned by the getter.
     *
     * FIXME this is duplicate behaviour to AddInitialisationCall, but restricted to a single parameter setter, but accepting also a non member function as the setter...
     *
     * This call uses a seperate getter and setter for serialisation and
     * deserialisation respectively. This is necessary when the value is not a
     * public member of the type.
     *
     * Optionally a label can be set, if it is not set then a unique label will
     * be generated automatically.
     *
     * Optionally additional validation can be added. This does not remove the
     * default validation.
     */
    template <typename Getter, typename Setter>
    requires std::is_invocable_v<Getter>
          && std::is_invocable_v<Setter, T&, ReturnTypeNoCVRef<Getter>>
    static void RegisterVariable(Getter&& getter, Setter&& setter, std::optional<std::string>&& label = std::nullopt, std::optional<std::function<bool(const ReturnTypeNoCVRef<Getter>&)>>&& customValidator = std::nullopt)
    {
        helper_.RegisterVariable(std::forward<Getter>(getter), std::forward<Setter>(setter), std::move(label), std::move(customValidator));
    }

    ///
    /// DefinePostSerialiseAction & DefinePostDeserialiseAction
    /// -------------------------------------------------------
    ///
    /// The intent here was to allow comlete user control over how their types
    /// were serialised/deserialised, whilst keeping the above helper functions
    /// easy to use.
    ///
    /// The following functions basically run as the very last step of serialise
    /// and deserialise respectively. They give the user complete control over
    /// the final result of each operation.
    ///

    /**
     * Accepts a function that has complete access to the serialised data
     * generated by EasySerDes so that any custom modification can be applied as
     * a final step for serialisation. These modifications can then be used in
     * DefinePostDeserialiseAction.
     *
     * FIXME this breaks Validation if the user actually adds anything... defeats the point, so need to track user changes and facilitate them in Validate
     */
    static void DefinePostSerialiseAction(std::function<void (const T&, nlohmann::json&)>&& action)
    {
        helper_.DefinePostSerialiseAction(std::move(action));
    }

    /**
     * Accepts a function that has complete access to the instance of T created
     * by EasySerDes during deserialisation, so that any custom modifications
     * can be applied as a final step of deserialisation. The serialised data is
     * available to read, in case the user placed custom values there via
     * DefinePostSerialiseAction.
     */
    static void DefinePostDeserialiseAction(std::function<void (const nlohmann::json&, T&)>&& action)
    {
        helper_.DefinePostDeserialiseAction(std::move(action));
    }

    ///
    /// Validate, Serialise, and Deserialise
    /// ------------------------------------
    ///
    /// These are declared publically so that when the user specialises a
    /// esd::Serialiser that extends this type, they only need to implement the
    /// Configure method.
    ///

    /**
     * Intended to be called by esd::Validate, or esd::Deserialise
     */
    static bool Validate(const nlohmann::json& serialised)
    {
        return helper_.Validate(serialised);
    }

    /**
     * Intended to be called by esd::Serialise
     */
    static nlohmann::json Serialise(const T& value)
    {
        return helper_.Serialise(value);
    }

    /**
     * Intended to be called by esd::Deserialise, or
     * esd::DeserialiseWithoutChecks
     */
    static T Deserialise(const nlohmann::json& serialised)
    {
        return helper_.Deserialise(serialised);
    }

    ///
    /// DeserialiseInPlace
    /// ------------------
    ///
    /// The intent here is to allow advanced users to re-use all of the code
    /// here for constructing a user type, but to construct that user type via
    /// a factory that doesn't return an intance of the type.
    ///
    /// This is important for supporting serialisation of boxed types like smart
    /// pointers, because it doesn't require the user type to be copied or moved
    /// after construction. It can also be used as an optimisation for
    /// containers that have emplace_back functionality, again preventing the
    /// need for a move or copy.
    ///
    /// Importantly, once the factory has been used to construct an instance of
    /// the type, EasySerDes still needs to access it in order to complete any
    /// additional initialise calls, or set any other registered variables.
    ///
    /// It is also important to note that these will almost always be used in a
    /// templated setting, where the user-type is unknown, and therefore the
    /// construction arguments are also unknown. This can be side-stepped by
    /// passing a templated lambda as the `factory` parameter:
    /// `[](auto... args){ return factory<T>(args...); }`
    ///

    /**
     * Accepts a callable that will somehow create an instance of type `T` and
     * return something that can be de-referenced to a reference of type T. If
     * the factory itself cannot do this, it should be wrapped in a lambda that
     * forwards the call to the factory and then gets a pointer to the instance
     * via other means.
     *
     * It also accepts a const reference to the json that will be used to
     * generate the input for the factory function.
     */
    template <typename Invocable>
    requires std::is_invocable_v<Invocable, ConstructionArgs...>
          && internal::TypeIsDereferencableFrom<T, ReturnTypeNoCVRef<Invocable, ConstructionArgs...>>
          && internal::IsComparableToNullptr<ReturnTypeNoCVRef<Invocable, ConstructionArgs...>>
    [[nodiscard]] static auto DeserialiseInPlace(Invocable factory, const nlohmann::json& toDeserialise) -> ReturnTypeNoCVRef<Invocable, ConstructionArgs...>
    {
        return helper_.DeserialiseInPlace(std::forward<Invocable>(factory), toDeserialise, std::index_sequence_for<ConstructionArgs...>());
    }

private:
    using HelperType = internal::Implementation<T, ConstructionArgs...>;
    static inline HelperType helper_ = [](){ HelperType h; Serialiser<T>::Configure(); return h; }();
};

namespace internal {
    // https://stackoverflow.com/questions/70130735/c-concept-to-check-for-derived-from-template -specialization
    template <template <class...> class Z, class... Args>
    void is_derived_from_specialization_of(const Z<Args...>&);

    template <class T, template <class...> class Z>
    concept IsDerivedFromSpecialisationOf = requires(const T& t) {
        is_derived_from_specialization_of<Z>(t);
    };
} // end namespace internal

// FIXME would be nicer to have this at the top!
template <typename T>
concept HasClassHelperSpecialisation = internal::IsDerivedFromSpecialisationOf<Serialiser<T>, ClassHelper>;

namespace internal {

///
///
/// This helper type is intentionally inaccesible to the user.
///
/// TODO the below need inline code comments that explain why I've done things a
/// certain way (at the very least for future me!)
///
template <typename T, typename... ConstructionArgs>
requires std::is_class_v<T>
class Implementation {
private:
    /*
     * This struct will store the information required for each member variable
     * of the user type to be read, written & validated. It cannot be a template
     * because it needs to be stored in a generic container.
     */
    struct Variable {
    public:
        std::function<void (const T& source, nlohmann::json& target)> writer_;
        std::function<bool (const nlohmann::json& serialisedVariable)> validator_;
        std::function<void (const nlohmann::json& source, T& target)> parser_;
    };

public:
    Implementation()
        : constructionVariables_{}
        , constructor_(nullptr)
        , initialisationCalls_{}
        , variables_{}
        , interdependantVariablesValidators_{}
        , postSerialisationAction_([](const T&, nlohmann::json&) -> void { /* do nothing by default */})
        , postDeserialisationAction_([](const nlohmann::json&, T&) -> void { /* do nothing by default */})
    {
        // Removes the need for the user to call an empty SetConstruction()
        // in cases where the type is default constructabe and the state is
        // managed by other means
        if constexpr(std::is_default_constructible_v<T>) {
            constructor_ = [](const nlohmann::json&) -> T { return {}; };
        }
    }

    [[nodiscard]] bool Validate(const nlohmann::json& json)
    {
        bool valid = json.is_object();
        if (valid) {
            for (const auto& [ key, jsonValue ] : json.items()) {
                if (variables_.count(key) == 1) {
                    if (!variables_.at(key).validator_(jsonValue)) {
                        // Value's custom validator failed
                        valid = false;
                    }
                } else {
                    // Invalid key detected
                    valid = false;
                }
            }
        }
        if (valid) {
            for (const auto& validator : interdependantVariablesValidators_) {
                valid = valid && std::invoke(validator, json);
            }
        }
        return valid;
    }

    [[nodiscard]] nlohmann::json Serialise(const T& toSerialise)
    {
        nlohmann::json serialised = nlohmann::json::object();
        for (const auto& [ key, memberHelper ] : variables_) {
            memberHelper.writer_(toSerialise, serialised);
        }
        std::invoke(postSerialisationAction_, toSerialise, serialised);
        return serialised;
    }

    [[nodiscard]] T Deserialise(const nlohmann::json& toDeserialise)
    {
        T deserialised = constructor_(toDeserialise);
        for (auto& initialiser : initialisationCalls_) {
            std::invoke(initialiser, toDeserialise, deserialised);
        }
        for (const auto& [ key, memberHelper ] : variables_) {
            memberHelper.parser_(toDeserialise.at(key), deserialised);
        }
        std::invoke(postDeserialisationAction_, toDeserialise, deserialised);
        return deserialised;
    }

    template <typename Factory, size_t... Indexes>
    requires std::is_invocable_v<Factory, ConstructionArgs...>
          && TypeIsDereferencableFrom<T, ReturnTypeNoCVRef<Factory, ConstructionArgs...>>
          && (sizeof...(ConstructionArgs) == sizeof...(Indexes))
    [[nodiscard]] auto DeserialiseInPlace(Factory factory, const nlohmann::json& toDeserialise, std::index_sequence<Indexes...>) -> ReturnTypeNoCVRef<Factory, ConstructionArgs...>
    {
        using ReturnType = ReturnTypeNoCVRef<Factory, ConstructionArgs...>;

        ReturnType retVal = std::invoke(factory, Serialiser<ConstructionArgs>::Deserialise(toDeserialise.at(constructionVariables_.at(Indexes)))...);

        if (retVal != nullptr) {
            retVal = std::invoke(factory, Serialiser<ConstructionArgs>::Deserialise(toDeserialise.at(constructionVariables_.at(Indexes)))...);
            T& deserialised = *retVal;
            for (auto& initialiser : initialisationCalls_) {
                std::invoke(initialiser, toDeserialise, deserialised);
            }
            for (const auto& [ key, memberHelper ] : variables_) {
                memberHelper.parser_(toDeserialise.at(key), deserialised);
            }
            std::invoke(postDeserialisationAction_, toDeserialise, deserialised);
        }

        return retVal;
    }

    void SetConstruction(Parameter<std::remove_cvref_t<ConstructionArgs>>... params)
    {
        constructor_ = [=](const nlohmann::json& serialised) -> T
        {
            return T{ Serialiser<std::remove_cvref_t<ConstructionArgs>>::Deserialise(serialised.at(params.parameterKey_))... };
        };

        (this->constructionVariables_.push_back(params.parameterKey_), ...);
    }

    template <typename Validator>
    requires std::is_invocable_r_v<bool, Validator, const std::remove_cvref_t<ConstructionArgs>&...>
    void SetConstruction(Parameter<std::remove_cvref_t<ConstructionArgs>>... params, Validator parameterValidator)
    {
        interdependantVariablesValidators_.push_back([=, validator = std::forward<Validator>(parameterValidator)](const nlohmann::json& serialised) -> bool
        {
            return std::invoke(validator, (Serialiser<std::remove_cvref_t<ConstructionArgs>>::Deserialise(serialised.at(params.parameterKey_)))...);
        });

        SetConstruction(std::move(params)...);
    }

    template <typename... FactoryArgs, typename Factory>
    requires std::is_invocable_r_v<T, Factory, FactoryArgs...>
    void SetConstruction(Parameter<std::remove_cvref_t<FactoryArgs>>... params, Factory factory)
    {
        constructor_ = [=, factory = std::forward<Factory>(factory)](const nlohmann::json& serialised) -> T
        {
            return std::invoke(factory, (Serialiser<std::remove_cvref_t<FactoryArgs>>::Deserialise(serialised.at(params.parameterKey_)))...);
        };

        (this->constructionVariables_.push_back(params.parameterKey_), ...);
    }

    template <typename... FactoryArgs, typename Factory, typename Validator>
    requires std::is_invocable_r_v<bool, Validator, const FactoryArgs&...>
          && std::is_invocable_r_v<T, Factory, FactoryArgs...>
    void SetConstruction(Parameter<std::remove_cvref_t<FactoryArgs>>... params, Factory factory, Validator parameterValidator)
    {
        interdependantVariablesValidators_.push_back([=, validator = std::forward<Validator>(parameterValidator)](const nlohmann::json& serialised) -> bool
        {
            return std::invoke(validator, (Serialiser<std::remove_cvref_t<FactoryArgs>>::Deserialise(serialised.at(params.parameterKey_)))...);
        });

        SetConstruction(std::forward<Factory>(factory), std::move(params)...);
    }

    template <typename MemberFunctionPointer, typename... Ts>
    requires std::is_member_function_pointer_v<MemberFunctionPointer>
          && std::is_invocable_v<MemberFunctionPointer, T&, Ts...>
    void AddInitialisationCall(MemberFunctionPointer initialisationCall, Parameter<Ts>... params)
    {
        initialisationCalls_.push_back([=](const nlohmann::json& serialised, T& target) -> void
        {
            std::invoke(initialisationCall, target, Serialiser<Ts>::Deserialise(serialised.at(std::string(params.parameterKey_)))...);
        });
    }

    /*
     * Specialisation to allow the user to define a validator that can cross
     * reference the construction parameters, useful to prevent crashes from
     * invalid construction.
     */

    template <typename Validator, typename MemberFunctionPointer, typename... Ts>
    requires std::is_member_function_pointer_v<MemberFunctionPointer>
          && std::is_invocable_v<MemberFunctionPointer, T&, Ts...>
          && std::is_invocable_r_v<bool, Validator, const Ts&...>
    void AddInitialisationCall(Validator parameterValidator, MemberFunctionPointer initialisationCall, Parameter<Ts>... params)
    {
        interdependantVariablesValidators_.push_back([=, validator = std::forward<Validator>(parameterValidator)](const nlohmann::json& serialised) -> bool
        {
            return std::invoke(validator, (Serialiser<Ts>::Deserialise(serialised.at(params.parameterKey_)))...);
        });

        AddInitialisationCall(initialisationCall, std::move(params)...);
    }

    template <class Invocable>
    requires std::is_invocable_v<Invocable, const T&>
    [[ nodiscard ]] Parameter<ReturnTypeNoCVRef<Invocable, const T&>> CreateParameter(Invocable&& valueGetter, std::optional<std::string>&& label = std::nullopt, std::optional<std::function<bool(const ReturnTypeNoCVRef<Invocable, const T&>&)>>&& customValidator = std::nullopt)
    {
        using ParamType = ReturnTypeNoCVRef<Invocable, const T&>;
        return RegisterVariableAsParameter<Invocable, ParamType>(std::move(valueGetter), std::move(label), std::move(customValidator));
    }

    template <class Invocable>
    requires std::is_invocable_v<Invocable>
    [[ nodiscard ]] Parameter<ReturnTypeNoCVRef<Invocable>> CreateParameter(Invocable&& valueGetter, std::optional<std::string>&& label = std::nullopt, std::optional<std::function<bool(const ReturnTypeNoCVRef<Invocable>&)>>&& customValidator = std::nullopt)
    {
        using ParamType = ReturnTypeNoCVRef<Invocable>;
        return RegisterVariableAsParameter<Invocable, ParamType>(std::move(valueGetter), std::move(label), std::move(customValidator));
    }

    template <typename ParamType>
    [[ nodiscard ]] Parameter<ParamType> CreateParameter(const ParamType& constValue, std::optional<std::string>&& label = std::nullopt)
    {
        return CreateParameter([=]() -> ParamType { return constValue; }, std::move(label), [=](const ParamType& v) -> bool { return v == constValue; });
    }

    template <class MemberObjectPointer>
    requires std::is_member_object_pointer_v<MemberObjectPointer>
          && std::is_invocable_v<MemberObjectPointer, T&>
    void RegisterVariable(MemberObjectPointer getterAndSetter, std::optional<std::string>&& label = std::nullopt, std::optional<std::function<bool(const ReturnTypeNoCVRef<MemberObjectPointer, T&>&)>>&& customValidator = std::nullopt)
    {
        using ParamType = ReturnTypeNoCVRef<MemberObjectPointer, T&>;

        auto setterWrapper = [getterAndSetter](const nlohmann::json& source, T& target)
        {
            std::invoke(getterAndSetter, target) = Serialiser<ParamType>::Deserialise(source);
        };

        RegisterVariableInternal<MemberObjectPointer, decltype(setterWrapper), ParamType>(std::move(getterAndSetter), std::move(setterWrapper), std::move(label), std::move(customValidator));
    }

    template <typename Getter, typename Setter>
    requires std::is_invocable_v<Getter, const T&>
          && std::is_invocable_v<Setter, T&, ReturnTypeNoCVRef<Getter, const T&>>
    void RegisterVariable(Getter&& getter, Setter&& setter, std::optional<std::string>&& label = std::nullopt, std::optional<std::function<bool(const ReturnTypeNoCVRef<Getter, const T&>&)>>&& customValidator = std::nullopt)
    {
        using ParamType = ReturnTypeNoCVRef<Getter, const T&>;

        RegisterVariableInternal<Getter, Setter, ParamType>(std::move(getter), [=](const nlohmann::json& source, T& target)
        {
            std::invoke(setter, target, Serialiser<ParamType>::Deserialise(source));
        }, std::move(label), std::move(customValidator));
    }

    template <typename Getter, typename Setter>
    requires std::is_invocable_v<Getter>
          && std::is_invocable_v<Setter, T&, ReturnTypeNoCVRef<Getter>>
    void RegisterVariable(Getter&& getter, Setter&& setter, std::optional<std::string>&& label = std::nullopt, std::optional<std::function<bool(const ReturnTypeNoCVRef<Getter>&)>>&& customValidator = std::nullopt)
    {
        using ParamType = ReturnTypeNoCVRef<Getter>;

        RegisterVariableInternal<Getter, Setter, ParamType>(std::move(getter), [=](const nlohmann::json& source, T& target)
        {
            std::invoke(setter, target, Serialiser<ParamType>::Deserialise(source));
        }, std::move(label), std::move(customValidator));
    }

    void DefinePostSerialiseAction(std::function<void (const T&, nlohmann::json&)>&& action)
    {
        postSerialisationAction_ = std::move(action);
    }

    void DefinePostDeserialiseAction(std::function<void (const nlohmann::json&, T&)>&& action)
    {
        postDeserialisationAction_ = std::move(action);
    }

private:
    std::vector<std::string> constructionVariables_;
    std::function<T (const nlohmann::json& serialised)> constructor_;
    std::vector<std::function<void (const nlohmann::json& serialised, T& target)>> initialisationCalls_;
    std::map<std::string, Variable> variables_;
    std::vector<std::function<bool (const nlohmann::json& serialised)>> interdependantVariablesValidators_;
    std::function<void (const T& t, nlohmann::json& serialised)> postSerialisationAction_;
    std::function<void (const nlohmann::json& serialised, T& t)> postDeserialisationAction_;

    // Not at all optimal but can never be a program bottleneck so might as well be clear
    std::string GenerateUniqueKey(const std::string& prefix) const
    {
        unsigned index = 0;
        auto NextKey = [&]() -> std::string
        {
            return prefix + std::to_string(index++);
        };

        std::string key;
        do {
            key = NextKey();
        } while (variables_.count(key) != 0);

        return key;
    }

    template <typename Invocable, typename ParameterType>
    requires std::is_invocable_r_v<ParameterType, Invocable, const T&> || std::is_invocable_r_v<ParameterType, Invocable>
    [[ nodiscard ]] Parameter<ParameterType> RegisterVariableAsParameter(Invocable&& valueGetter, std::optional<std::string>&& label = std::nullopt, std::optional<std::function<bool(const ParameterType&)>>&& customValidator = std::nullopt)
    {
        if (!label.has_value()) {
            // User doesn't care about labels, but we need unique keys for the JSON
            label = GenerateUniqueKey("param");
        } else if (variables_.count(label.value()) != 0) {
            // User specified the same label twice, but we need unique keys for the JSON
            // Simply modify it for minimal disruption/confusion
            label = GenerateUniqueKey(label.value());
        }

        variables_.insert(std::make_pair(label.value(), Variable{
                                             { [=](const T& source, nlohmann::json& target)
                                               {
                                                   if constexpr (std::is_invocable_v<Invocable, const T&>) {
                                                       target[label.value()] = Serialiser<ParameterType>::Serialise(std::invoke(valueGetter, source));
                                                   } else if constexpr (std::is_invocable_v<Invocable>) {
                                                       target[label.value()] = Serialiser<ParameterType>::Serialise(std::invoke(valueGetter));
                                                   }
                                               } },
                                             { [=, customValidator = std::move(customValidator)](const nlohmann::json& serialisedVariable) -> bool
                                               {
                                                   bool hasCustomValidator = customValidator.has_value();
                                                   return Serialiser<ParameterType>::Validate(serialisedVariable) && (!hasCustomValidator || std::invoke(customValidator.value(), Serialiser<ParameterType>::Deserialise(serialisedVariable)));
                                               } },
                                             { [=](const nlohmann::json&, T&)
                                               {
                                                   // Do nothing, this value is handled during construction or a function call
                                               } }
                                         }));
        return Parameter<ParameterType>{ label.value() };
    }

    template <typename InvocableGetter, typename InvocableSetter, typename ParameterType>
    requires (std::is_invocable_r_v<ParameterType, InvocableGetter, const T&> || std::is_invocable_r_v<ParameterType, InvocableGetter>)
          && std::is_invocable_r_v<void, InvocableSetter, const nlohmann::json&, T&>
    void RegisterVariableInternal(InvocableGetter&& valueGetter, InvocableSetter&& valueSetter, std::optional<std::string>&& label = std::nullopt, std::optional<std::function<bool(const ParameterType&)>>&& customValidator = std::nullopt)
    {
        if (!label.has_value()) {
            // User doesn't care about labels, but we need unique keys for the JSON
            label = GenerateUniqueKey("param");
        } else if (variables_.count(label.value()) != 0) {
            // User specified the same label twice, but we need unique keys for the JSON
            // Simply modify it for minimal disruption/confusion
            label = GenerateUniqueKey(label.value());
        }

        variables_.insert(std::make_pair(std::move(label.value()), Variable{
                                             { [=](const T& source, nlohmann::json& target)
                                               {
                                                   if constexpr (std::is_invocable_v<InvocableGetter, const T&>) {
                                                       target[label.value()] = Serialiser<ParameterType>::Serialise(std::invoke(valueGetter, source));
                                                   } else if constexpr (std::is_invocable_v<InvocableGetter>) {
                                                       target[label.value()] = Serialiser<ParameterType>::Serialise(std::invoke(valueGetter));
                                                   }
                                               } },
                                             { [=, customValidator = std::move(customValidator)](const nlohmann::json& serialisedVariable) -> bool
                                               {
                                                   bool hasCustomValidator = customValidator.has_value();
                                                   return Serialiser<ParameterType>::Validate(serialisedVariable) && (!hasCustomValidator || std::invoke(customValidator.value(), Serialiser<ParameterType>::Deserialise(serialisedVariable)));
                                               } },
                                             std::move(valueSetter)
                                         }));
    }
};

} // end namespace internal

} // end namespace esd

#endif // EASYSERDESCLASSHELPER_H
