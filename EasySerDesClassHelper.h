#ifndef EASYSERDESCLASSHELPER_H
#define EASYSERDESCLASSHELPER_H

#include "EasySerDesCore.h"
#include "EasySerDesJsonHelpers.h"

#include <nlohmann/json.hpp>

#include <type_traits>
#include <assert.h>

namespace esd {

/**
 * This rename is to help make function signatures easier to read
 */
template <typename F, typename... Args>
using ReturnTypeNoCVRef = std::remove_cvref_t<std::invoke_result_t<F, Args...>>;

/**
 * Forward declaration of helper type, because it was desirable to define
 * JsonClassSerialiser first.
 */
template <typename T, typename... ConstructionArgs>
requires std::is_class_v<T>
class InternalHelper;

/**
 * @brief The JsonClassSerialiser class should be extended by the user in order
 * to allow CppEasySerDes to support custom user types.
 *
 * T                Is the type to be supported.
 *
 * ConstructionArgs Are the parameters required to create an instance of the
 *                  type, via one of brace initialisation, a constructor, or a
 *                  factory function call.
 *
 * The main aim of this class is to allow the user to define a single function
 * in order to support Serialise, Deserialise and Validate.
 *
 * It has also expanded to include support for building of the type "in place",
 * making std::make_shared, std::make_unique, emplace etc. possible targets for
 * deserialisation.
 */
template <typename T, typename... ConstructionArgs>
requires std::is_class_v<T> && (std::is_default_constructible_v<T> || std::constructible_from<T, ConstructionArgs...>)
class JsonClassSerialiser {
public:
    // FIXME doesn't work when T is a templated type!
    using HelperType = esd::InternalHelper<T, ConstructionArgs...>;

    template <template<typename...> class Func, typename... AdditionalArgs>
    using ConstructionArgsForwarder = Func<AdditionalArgs..., ConstructionArgs...>;

    static bool Validate(const nlohmann::json& serialised)
    {
        return helper_.Validate(serialised);
    }

    static nlohmann::json Serialise(const T& value)
    {
        return helper_.Serialise(value);
    }

    static T Deserialise(const nlohmann::json& serialised)
    {
        return helper_.Deserialise(serialised);
    }

    // TODO work out if it is better to use std::function than a templated Invocable type with constraints, (and if so, better to take std::function const ref or rvalue?)
    // is making the signature more verbose less readable than having a template with constraints?!
    // should I be avoiding templates if I don't need them?
    template <typename Invocable>
    [[nodiscard]] ReturnTypeNoCVRef<Invocable, ConstructionArgs...> Deserialise(Invocable factory, const nlohmann::json& toDeserialise)
    {
        return helper_.Deserialise(std::forward<Invocable>(factory), toDeserialise);
    }

    template <typename Invocable>
    [[nodiscard]] ReturnTypeNoCVRef<Invocable, ConstructionArgs...> Deserialise(Invocable factory, const std::function<T* (ReturnTypeNoCVRef<Invocable, ConstructionArgs...>&)>& accessResultFromFactoryOutput, const nlohmann::json& toDeserialise)
    {
        return helper_.Deserialise(std::forward<Invocable>(factory), accessResultFromFactoryOutput, toDeserialise);
    }

private:
    static inline HelperType helper_ = [](){ HelperType h; JsonSerialiser<T>::SetupHelper(h); return h; }();
};

///
/// \brief The InternalHelper class
///
/// This helper type is intentionally accesible to the user. This implementation
///
///
template <typename T, typename... ConstructionArgs>
requires std::is_class_v<T>
class InternalHelper {
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

    /*
     * This struct holds type information to assist template type typededuction
     * in various places
     */
    template <typename ParamType>
    struct Parameter {
    public:
        using value_t = ParamType;
        std::string parameterKey_;
    };

public:
    InternalHelper()
        : constructionVariables_{}
        , constructor_(nullptr)
        , initialisationCalls_{}
        , variables_{}
        , interdependantVariablesValidators_{}
        , postSerialisationAction_([](const T&, nlohmann::json&) -> void { /* do nothing by default */})
        , postDeserialisationAction_([](const nlohmann::json&, T&) -> void { /* do nothing by default */})
    {
        // Removes the need for the user to call an empty RegisterConstruction()
        // in cases where the type is default constructabe and the state is
        // managed by other means
        if constexpr(std::is_default_constructible_v<T>) {
            constructor_ = [](const nlohmann::json&) -> T { return {}; };
        }
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

    template <typename Factory>
    requires std::is_invocable_v<Factory, ConstructionArgs...>
          && std::same_as<T, ReturnTypeNoCVRef<Factory, ConstructionArgs...>>
    [[nodiscard]] ReturnTypeNoCVRef<Factory, ConstructionArgs...> Deserialise(Factory factory, const nlohmann::json& toDeserialise)
    {
        return Deserialise<Factory>(std::forward<Factory>(factory), [](T& result) -> T* { return &result; }, toDeserialise);
    }

    template <typename Factory>
    requires std::is_invocable_v<Factory, ConstructionArgs...>
          && TypeIsDereferencableFrom<T, ReturnTypeNoCVRef<Factory, ConstructionArgs...>>
    [[nodiscard]] ReturnTypeNoCVRef<Factory, ConstructionArgs...> Deserialise(Factory factory, const nlohmann::json& toDeserialise)
    {
        return Deserialise<Factory>(std::forward<Factory>(factory), [](auto& result) -> T* { return &*result; }, toDeserialise);
    }

    template <typename Factory>
    requires std::is_invocable_v<Factory, ConstructionArgs...>
    [[nodiscard]]  ReturnTypeNoCVRef<Factory, ConstructionArgs...> Deserialise(Factory factory, const std::function<T* (ReturnTypeNoCVRef<Factory, ConstructionArgs...>&)>& accessResultFromFactoryOutput, const nlohmann::json& toDeserialise)
    {
        using ReturnType = ReturnTypeNoCVRef<Factory, ConstructionArgs...>;

        size_t index = 0;
        ReturnType retVal =  std::invoke(factory, esd::DeserialiseWithoutChecks<ConstructionArgs>(toDeserialise.at(constructionVariables_.at(index++)))...);
        T* deserialised = std::invoke(accessResultFromFactoryOutput, retVal);
        for (auto& initialiser : initialisationCalls_) {
            std::invoke(initialiser, toDeserialise, *deserialised);
        }
        for (const auto& [ key, memberHelper ] : variables_) {
            memberHelper.parser_(toDeserialise.at(key), *deserialised);
        }
        std::invoke(postDeserialisationAction_, toDeserialise, *deserialised);
        return retVal;
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

    /**
     * @brief Validate Checks the supplied json object and ensures that it
     *                 contains all of the values required to fully initialise
     *                 an instance of type T.
     * @param json A JSON object, should have been createded using Serialise.
     * @return true if the json object can safely be Deserialised.
     */
    [[nodiscard]] bool Validate(const nlohmann::json& json)
    {
        bool valid = json.is_object();
        if (valid) {
            for (const auto& [ key, jsonValue ] : json.items()) {
                if (variables_.count(key) == 1) {
                    if (!variables_.at(key).validator_(jsonValue)) {
                        valid = false;
                    }
                } /* TODO not yet sure how polymorphic types are going to be supported! else if (key != "__typename") { // exception for __typename which is used for polymorphic types
                    valid = false;
                }*/
            }
        }
        if (valid) {
            for (const auto& validator : interdependantVariablesValidators_) {
                valid = valid && std::invoke(validator, json);
            }
        }
        return valid;
    }

    ///
    /// RegisterConstruction
    /// --------------------
    /// This is the primary function that the user of this helper should be
    /// calling for POD types, and types that handle their internal state via a
    /// constructor.
    ///
    /// The following templates require a series of templated Parameter<> types
    /// that can only be created by the user by calling CreateParameter. This
    /// pattern allows us to create a simple constructor that simply stores the
    /// keys required to fetch each construction parameter from the JSON, and
    /// allows us to easily check that the parameters match the construction
    /// signature, and deserialise the JSON into the correct type.
    ///
    ///
    /// FIXME the follwoing is a user guide and doesn't belong here
    ///
    /// This is the primary function that the user of this helper should be
    /// calling. It takes a number of 'Parameter<T>'s, a type which is hidden
    /// from the user, forcing the user to call
    /// RegisterConstruction(CreateParameter(...), CreateParameter(...), ...);
    /// Parameter<T> is essentially just a string, with a type, allowing easy
    /// type checking of the defined constructor against the main type
    ///
    /// Some usage examples:
    ///
    /// // Simple struct, will use brace initialisation under the hood
    /// struct Point3D {
    ///     double x, y, z;
    /// };
    ///
    /// HelperType& h;
    /// h.RegisterConstruction(h.CreateParameter(&Point3D::x), h.CreateParameter(&Point3D::y), h.CreateParameter(&Point3D::z));
    ///
    /// // More complex example, will call the constructor and set c seperately under the hood
    /// class Foo {
    /// public:
    ///     int c;
    ///
    ///     Foo(int a, int b);
    ///
    ///     int GetA() const;
    ///     int GetB() const;
    ///
    /// private:
    ///     int a, b;
    /// };
    ///
    /// HelperType& h;
    /// h.RegisterConstruction(h.CreateParameter(&Foo::GetA, "A"), h.CreateParameter(&Foo::GetB, "B"));
    /// h.RegisterVariable(&Foo::c, "C");
    ///
    /// // One of everything just to be complete
    /// float MoreMagic()
    /// {
    ///     return 3.14f;
    /// }
    ///
    /// class Contrived {
    /// public:
    ///     std::string name;
    ///
    ///     Contrived(int magic, float moreMagic, std::string name, std::vector<double> numbers, double cachedAverage);
    ///
    ///     const std::vector<double>& GetNumbers() const;
    ///
    ///     char GetChar() const;
    ///     void SetChar(char c) const;
    ///
    /// private:
    ///     std::vector<double> numbers;
    ///     char c;
    /// };
    ///
    /// HelperType& h;
    /// h.RegisterConstruction(h.CreateParameter([]() -> int { return 42 }, "TheAnswer"),
    ///                        h.CreateParameter(&MoreMagic),
    ///                        h.CreateParameter(&Contrived::name),
    ///                        h.CreateParameter(&Contrived::GetNumbers),
    ///                        h.CreateParameter([](const Contrived& c) -> double
    ///                                           {
    ///                                               return std::ranges::accumulate(c.GetNumbers(), 0.0) / c.GetNumbers().size();
    ///                                           },
    ///                                           "CachedAverage"
    ///                        );
    /// h.RegisterVariable(&Contrived::name, "name");
    /// h.RegisterVariable(&Contrived::GetChar, &Contrived::SetChar, "C", [](const char& c) -> bool { return c > 'a' && c < 'z'; });
    ///

    void RegisterConstruction(Parameter<std::remove_cvref_t<ConstructionArgs>>... params)
    {
        constructor_ = [=](const nlohmann::json& serialised) -> T
        {
            return { (esd::DeserialiseWithoutChecks<std::remove_cvref_t<ConstructionArgs>>(serialised.at(params.parameterKey_)))... };
        };

        (this->constructionVariables_.push_back(params.parameterKey_), ...);
    }

    template <typename Validator>
    requires std::is_invocable_r_v<bool, Validator, const std::remove_cvref_t<ConstructionArgs>&...>
    void RegisterConstruction(Parameter<std::remove_cvref_t<ConstructionArgs>>... params, Validator parameterValidator)
    {
        interdependantVariablesValidators_.push_back([=, validator = std::forward<Validator>(parameterValidator)](const nlohmann::json& serialised) -> bool
        {
            return std::invoke(validator, (esd::DeserialiseWithoutChecks<std::remove_cvref_t<ConstructionArgs>>(serialised.at(params.parameterKey_)))...);
        });

        RegisterConstruction(std::move(params)...);
    }

    template <typename Factory>
    requires std::is_invocable_r_v<T, Factory, ConstructionArgs...>
    void RegisterConstruction(Parameter<std::remove_cvref_t<ConstructionArgs>>... params, Factory factory)
    {
        constructor_ = [=, factory = std::forward<Factory>(factory)](const nlohmann::json& serialised) -> T
        {
            return std::invoke(factory, (esd::DeserialiseWithoutChecks<std::remove_cvref_t<ConstructionArgs>>(serialised.at(params.parameterKey_)))...);
        };

        (this->constructionVariables_.push_back(params.parameterKey_), ...);
    }

    template <typename Factory, typename Validator>
    requires std::is_invocable_r_v<bool, Validator, const ConstructionArgs&...>
          && std::is_invocable_r_v<T, Factory, ConstructionArgs...>
    void RegisterConstruction(Parameter<std::remove_cvref_t<ConstructionArgs>>... params, Factory factory, Validator parameterValidator)
    {
        interdependantVariablesValidators_.push_back([=, validator = std::forward<Validator>(parameterValidator)](const nlohmann::json& serialised) -> bool
        {
            return std::invoke(validator, (esd::DeserialiseWithoutChecks<std::remove_cvref_t<ConstructionArgs>>(serialised.at(params.parameterKey_)))...);
        });

        RegisterConstruction(std::move(params)..., std::forward<Factory>(factory));
    }

    ///
    /// RegisterInitialisation
    /// ----------------------
    /// This is the primary function that the user of this helper should be
    /// calling for types which do not handle their complete internal state via
    /// a constructor, but instead have one or more initialisation functions.
    /// In most cases this should not be necessary.
    ///
    /// The following templates require a series of templated Parameter<> types
    /// that can only be created by the user by calling CreateParameter. This
    /// pattern allows us to store a simple function call that simply stores the
    /// keys required to fetch each function parameter from the stored JSON, and
    /// allows us to easily check that the parameters match the function
    /// signature and deserialise the JSON into the correct type.

    template <typename MemberFunctionPointer, typename... Ts>
    requires std::is_member_function_pointer_v<MemberFunctionPointer>
          && std::is_invocable_v<MemberFunctionPointer, T&, Ts...>
    void RegisterInitialisation(MemberFunctionPointer initialisationCall, Parameter<Ts>... params)
    {
        initialisationCalls_.push_back([=](const nlohmann::json& serialised, T& target) -> void
        {
            std::invoke(initialisationCall, target, esd::DeserialiseWithoutChecks<Ts>(serialised.at(std::string(params.parameterKey_)))...);
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
    void RegisterInitialisation(Validator&& parameterValidator, MemberFunctionPointer initialisationCall, Parameter<Ts>... params)
    {
        interdependantVariablesValidators_.push_back([=, validator = std::move(parameterValidator)](const nlohmann::json& serialised) -> bool
        {
            return std::invoke(validator, (esd::DeserialiseWithoutChecks<Ts>(serialised.at(params.parameterKey_)))...);
        });

        RegisterInitialisation(initialisationCall, std::move(params)...);
    }

    ///
    /// CreateParameter
    /// ---------------------
    ///
    /// FIXME the follwoing is a user guide and doesn't belong here
    ///
    /// The following template specialisations allow the user to define how the
    /// individual construction parameters are calculated. The templates accept
    /// the following:
    ///
    ///  1. A member pointer, you can specify either a public member via
    ///     &MyType::memberName_ or a public member function via
    ///     &MyType::GetMember
    ///  2. Via a custom callback, which can be a lambda or function pointer
    ///     that accepts a reference to the target to be serialised and provides
    ///     a value. The reference may be unused if the value will be decided by
    ///     some other global value.
    ///  3. Via a constant value, this will automatically add a validation check
    ///     to ensure the deserialised value matches the hard coded constant.
    ///
    /// As well as the "getter", the user can optionally specify a unique label
    /// (to make the serialised JSON more human readable) and a custom validator
    /// which they can use to constrain the valid set of values.
    ///
    /// Some usage examples:
    ///
    /// // Assuming T has a public member "double miles_;"
    /// CreateParameter(&T::miles_, "miles", [](const double& miles) -> bool {
    ///     // Sanity check the number of miles the car has travelled (world record is 2.5 million)
    ///     return miles > 0.0 && miles < 10'000'000.0;
    /// });
    ///
    /// // Assuming T has a public function "bool HasWindshield() const;"
    /// CreateParameter(&T::HasWindshield, "hasWindshield");
    ///
    /// // With a lambda that takes a "const T&"
    /// CreateParameter([](const T& t) -> size_t { return t.GetBuffer().capacity(); }, "reservedCount");
    /// OR
    /// auto getter = [](const T& t) -> size_t { return t.GetBuffer().capacity(); };
    /// CreateParameter(std::move(getter), "reservedCount");
    ///
    /// // With a lambda that takes no arguments
    /// CreateParameter([]() -> size_t { return 42; }, "reservedCount");
    ///
    /// // With a global function that takes no arguments
    /// size_t SomeGlobalFunc(const T& t)
    /// {
    ///     return t.GetBuffer().capacity();
    /// }
    /// CreateParameter(&SomeGlobalFunction, "reservedCount");
    ///

    ///
    /// Overload for member pointers, and function pointers/lambdas that accept
    /// a "const T&" parameter
    ///
    template <class Invocable>
    requires std::is_invocable_v<Invocable, const T&>
    [[ nodiscard ]] Parameter<ReturnTypeNoCVRef<Invocable, const T&>> CreateParameter(Invocable&& valueGetter, std::optional<std::string>&& label = std::nullopt, std::optional<std::function<bool(const ReturnTypeNoCVRef<Invocable, const T&>&)>>&& customValidator = std::nullopt)
    {
        using ParamType = ReturnTypeNoCVRef<Invocable, const T&>;
        return RegisterVariableAsParameter<Invocable, ParamType>(std::move(valueGetter), std::move(label), std::move(customValidator));
    }

    ///
    /// Overload for function pointers/lambdas that accept no parameters
    ///
    template <class Invocable>
    requires std::is_invocable_v<Invocable>
    [[ nodiscard ]] Parameter<ReturnTypeNoCVRef<Invocable>> CreateParameter(Invocable&& valueGetter, std::optional<std::string>&& label = std::nullopt, std::optional<std::function<bool(const ReturnTypeNoCVRef<Invocable>&)>>&& customValidator = std::nullopt)
    {
        using ParamType = ReturnTypeNoCVRef<Invocable>;
        return RegisterVariableAsParameter<Invocable, ParamType>(std::move(valueGetter), std::move(label), std::move(customValidator));
    }

    ///
    /// Overload for when the user just wants to specify a magic value
    ///
    template <typename ParamType>
    [[ nodiscard ]] Parameter<ParamType> CreateParameter(const ParamType& constValue, std::optional<std::string>&& label = std::nullopt)
    {
        return CreateParameter([=]() -> ParamType { return constValue; }, std::move(label), [=](const ParamType& v) -> bool { return v == constValue; });
    }

    ///
    /// RegisterVariable
    /// ----------------
    /// The following template specialisations allow the user to register values
    /// that need to be saved/loaded, but are not handles during construction.
    /// In most cases these should not be necessary.
    ///

    ///
    /// Overload for member object pointers
    ///
    template <class MemberObjectPointer>
    requires std::is_member_object_pointer_v<MemberObjectPointer>
          && std::is_invocable_v<MemberObjectPointer, T&>
    void RegisterVariable(MemberObjectPointer getterAndSetter, std::optional<std::string>&& label = std::nullopt, std::optional<std::function<bool(const ReturnTypeNoCVRef<MemberObjectPointer, T&>&)>>&& customValidator = std::nullopt)
    {
        using ParamType = ReturnTypeNoCVRef<MemberObjectPointer, T&>;

        auto setterWrapper = [getterAndSetter](const nlohmann::json& source, T& target)
        {
            std::invoke(getterAndSetter, target) = esd::DeserialiseWithoutChecks<ParamType>(source);
        };

        RegisterVariableInternal<MemberObjectPointer, decltype(setterWrapper), ParamType>(std::move(getterAndSetter), std::move(setterWrapper), std::move(label), std::move(customValidator));
    }

    ///
    /// Overload for a getter that accepts a "const T&" parameter, and any
    /// setter that accepts a reference to the target, and a reference to the
    /// deserialised value.
    ///
    template <typename Getter, typename Setter>
    requires std::is_invocable_v<Getter, const T&>
          && std::is_invocable_v<Setter, T&, const ReturnTypeNoCVRef<Getter, const T&>&>
    void RegisterVariable(Getter&& getter, Setter&& setter, std::optional<std::string>&& label = std::nullopt, std::optional<std::function<bool(const ReturnTypeNoCVRef<Getter, const T&>&)>>&& customValidator = std::nullopt)
    {
        using ParamType = ReturnTypeNoCVRef<Getter, const T&>;

        RegisterVariableInternal<Getter, Setter, ParamType>(std::move(getter), [=](const nlohmann::json& source, T& target)
        {
            std::invoke(setter, target, esd::DeserialiseWithoutChecks<ParamType>(source));
        }, std::move(label), std::move(customValidator));
    }

    ///
    /// Overload for a getter that accepts no parameters, and any setter that
    /// accepts a reference to the target, and a reference to the deserialised
    /// value.
    ///
    template <typename Getter, typename Setter>
    requires std::is_invocable_v<Getter>
          && std::is_invocable_v<Setter, T&, const ReturnTypeNoCVRef<Getter>&>
    void RegisterVariable(Getter&& getter, Setter&& setter, std::optional<std::string>&& label = std::nullopt, std::optional<std::function<bool(const ReturnTypeNoCVRef<Getter>&)>>&& customValidator = std::nullopt)
    {
        using ParamType = ReturnTypeNoCVRef<Getter>;

        RegisterVariableInternal<Getter, Setter, ParamType>(std::move(getter), [=](const nlohmann::json& source, T& target)
        {
        std::invoke(setter, target, esd::DeserialiseWithoutChecks<ParamType>(source));
    }, std::move(label), std::move(customValidator));
    }

    ///
    /// DefinePostSerialiseAction
    /// ----------------
    ///

    void DefinePostSerialiseAction(std::function<void (const T&, nlohmann::json&)>&& action)
    {
        postSerialisationAction_ = std::move(action);
    }

    ///
    /// DefinePostDeserialiseAction
    /// ----------------
    ///

    void DefinePostDeserialiseAction(std::function<void (const nlohmann::json&, T&)>&& action)
    {
        postDeserialisationAction_ = std::move(action);
    }

    // FLASH OF BRILLIANCE ... maybe? might have been the fog hiding the issues
    // might make registering default constructible types better and solve our shared_ptr factory conundrum
    // removes nice naming of member types though...
    /*
    template <typename T, typename... Args>
    {
        use { esd::Deserialise<Args>(json.at(index++)), ... } to initialise
        use auto [  ] or tuple to somehow get members and serialise (might need to specialise template funct)

        // then we can support shared_ptr amongst other things
        template <typename ReturnType, typename FactoryFunction>
        requires SomeCleverChecksToMakeSureAllIsWell
        ReturnType ConstructInPlace(FactoryFunction& constructor)
        {
            return constructor(esd::Deserialise<Args>(json.at(index++)), ...);
        }
    };
     */


    /// This factory bit here, needs to be more accesible for types such as shared pointer, that need to take the constituents of some other type and emplace them into its own factory function...
    ///
    /// this leads me to think there's a more generic way to support all types perhaps?
    ///
    /// after all an int can be brace initialised from an int and they may be a shared_ptr<int> that needs to be factory made, so plausibly any type
    /// built in or user may need some way of taking the construction parameters and applying them to something else (also emplacing into containers may be required for certain types)
    ///
    /// Currently anything can only access a completely deserialised type, perhaps the generic base impl needs an understanding of the types required to construct it
    ///
    /// So perhaps a map of all supported types is the way to go, built-in or else, the mapped type would be able to accept some "thing" i.e. {}, T, factFunc, and produce the desired type
    ///
    ///
    /// template <typename T, ConstructionTypeEnum E, typename... ConstructorSignature>
    /// class typeHelper {
    /// public:
    ///     using ConstructorSignatureArgs = ConstructorSignature...;
    ///
    ///     template <typename ReturnType, typename Callable>
    ///     ReturnType Emplace(Callable& c, json j)
    ///     {
    ///         c(Deserialise<ConstructorSignature>(j needs to be split up somehow),...);
    ///     }
    /// };

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
                                                       if (customValidator.has_value()) {
                                                           // FIXME we shouldn't wait till load time to know we've saved an invalid value! but an assert feels wrong...
                                                           assert(std::invoke(customValidator.value(), std::invoke(valueGetter, source)));
                                                       }
                                                       target[label.value()] = esd::Serialise<ParameterType>(std::invoke(valueGetter, source));
                                                   } else if constexpr (std::is_invocable_v<Invocable>) {
                                                       if (customValidator.has_value()) {
                                                           // FIXME we shouldn't wait till load time to know we've saved an invalid value! but an assert feels wrong...
                                                           assert(std::invoke(customValidator.value(), std::invoke(valueGetter)));
                                                       }
                                                       target[label.value()] = esd::Serialise<ParameterType>(std::invoke(valueGetter));
                                                   }
                                               } },
                                             { [=, customValidator = std::move(customValidator)](const nlohmann::json& serialisedVariable) -> bool
                                               {
                                                   bool hasCustomValidator = customValidator.has_value();
                                                   return esd::Validate<ParameterType>(serialisedVariable) && (!hasCustomValidator || std::invoke(customValidator.value(), esd::DeserialiseWithoutChecks<ParameterType>(serialisedVariable)));
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
                                                       if (customValidator.has_value()) {
                                                           // FIXME we shouldn't wait till load time to know we've saved an invalid value! but an assert feels wrong...
                                                           assert(std::invoke(customValidator.value(), std::invoke(valueGetter, source)));
                                                       }
                                                       target[label.value()] = esd::Serialise<ParameterType>(std::invoke(valueGetter, source));
                                                   } else if constexpr (std::is_invocable_v<InvocableGetter>) {
                                                       if (customValidator.has_value()) {
                                                           // FIXME we shouldn't wait till load time to know we've saved an invalid value! but an assert feels wrong...
                                                           assert(std::invoke(customValidator.value(), std::invoke(valueGetter)));
                                                       }
                                                       target[label.value()] = esd::Serialise<ParameterType>(std::invoke(valueGetter));
                                                   }
                                               } },
                                             { [=, customValidator = std::move(customValidator)](const nlohmann::json& serialisedVariable) -> bool
                                               {
                                                   bool hasCustomValidator = customValidator.has_value();
                                                   return esd::Validate<ParameterType>(serialisedVariable) && (!hasCustomValidator || std::invoke(customValidator.value(), esd::DeserialiseWithoutChecks<ParameterType>(serialisedVariable)));
                                               } },
                                             std::move(valueSetter)
                                         }));
    }
};

} // end namespace esd

#endif // EASYSERDESCLASSHELPER_H
