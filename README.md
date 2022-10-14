# CppEasySerDes
A library for easy conversion of C++ types to/from JSON, allowing for a concise single definition that handles serialisation, deserialisation and validation.

## Disclaimer
-------------
I'm currently suffering from pretty severe long covid brain fog.
I hashed out the vast majority of the pre-covid and have been using the polishing of this project to gauge my recovery.
I'm not a proud person, if I've made wildly inaccurate claims, or my implementation is dumb, or I've missed something obvious, I'd be thrilled to know! =]
Also, this isn't designed to be fast, it is designed to be convinient, terse, and have human readable output.

## Why?
-------
 - A lot of solutions for this problem require you define two functions, a reader and a writer. These are often identical functions but in reverse and must be maintained in tandem, violating the DRY principle.
 - Adding validation or constraints to the acceptable values stored sucks because you need to add the same checks to both the writer and the reader functions.
 - When using a storage structure like JSON, you must now either create variables for each key, so they an be shared between the reader/writer functions, or carefully use the same magic values in both.
 - And now you want to deserialise it into a smart pointer without copying, or emplace_back into an array? Well you'll have to duplicate your deserialisation function!

## Usage Examples
-----------------
The most basic example, with and without optional extras.
```` C++
struct TrivialType {
    int a_;
    std::vector<int> b_;
    std::string c_;
};

template<>
struct JsonSerialiser<TrivialTestType> : public JsonClassSerialiser<TrivialTestType, int, std::vector<int>, std::string> {
    static void SetupHelper(HelperType& h)
    {
        // Here we pass in static member object pointers that can be applied to an instance of the type later
        h.RegisterConstruction(h.CreateParameter(&TrivialTestType::a_), 
                               h.CreateParameter(&TrivialTestType::b_),
                               h.CreateParameter(&TrivialTestType::c_));
        
        // Optionally, any parameter or variable can have a label and custom validator
        h.RegisterConstruction(h.CreateParameter(&TrivialTestType::a_, "A", []() -> bool { /* ... */ }), 
                               h.CreateParameter(&TrivialTestType::b_, "B", []() -> bool { /* ... */ }),
                               h.CreateParameter(&TrivialTestType::c_, "C", []() -> bool { /* ... */ }));
    }
};

// I have plans for the following, which would reduce boilerplate for POD types greatly
template<>
struct JsonSerialiser<TrivialTestType> : public JsonPodSerialiser<TrivialTestType, int, std::vector<int>, std::string> {};
````
A contrived example to show off more features. (Class definitions not included for brevity)
```` C++
class ContrivedType {
public:
    std::string a_;
    ContrivedType(int b);
    
    // Our constructor doesn't handle all of the setup
    void Initialise(std::string a, double c);
    
    // Neither the Constructor nor the Initialise call cover member d_
    void SetD(TrivialType&& d);
    
    // Members b_ and c_ aren't publically visible    
    int GetB() const;
    const double& GetC() const;

    // No simple Getter for member d_
    const int& GetD_A() const;
    const std::vector<int>& GetD_B() const;
    const std::string& GetD_C();
    
private:
    int b_;
    double c_;
    TrivialType d_;
};

template<>
struct JsonSerialiser<ContrivedType> : public JsonClassSerialiser<ContrivedType, int> {
    static void SetupHelper(HelperType& h)
    {
        // Note that registering construction isn't necessary for default constructable types
        // Here we use a static member function pointer instead of a member object pointer
        h.RegisterConstruction(h.CreateParameter(&ContrivedType::GetB));
        
        // Some types require multi-parameter initialisation calls that aren't the constructor
        h.RegisterInitialisation(h.CreateParameter(&ContrivedType::a_), h.CreateParameter(&ContrivedType::GetC));
        
        // Custom Getter or Setters can be provided for any variable or parameter, not only member pointers are supported
        auto customGetD = [](const ContrivedType::& toSerialise) -> TrivialTestType 
                          {
                              return { toSerialise.GetD_A(), toSerialise.GetD_B(), toSerialise.GetD_C() };  
                          }
        // Register a variable that isn't covered in construction or via an initialiser (note that h.CreateParameter calls aren't necessary here)
        h.RegisterVariable(std::move(customGetD), &ContrivedType::SetD);
        
        // The follwoing make sure that you still have full flexibility (apart from registering the construction for non default constructable types)
        
        // Perhaps add something custom to the json, or do global housekeeping
        h.DefinePostSerialiseAction([](const T& input, nlohmann::json& output){ /* ... */ });
        // Perhaps parse something custom from the json, or validate the output, or do global housekeeping
        h.DefinePostDeserialiseAction([](const nlohmann::json& input, T& output){ /* ... */ });
    }
};
````

## Known Limitations
-----------------
 - Polymorphism isn't yet supported, but this is planned and roughly implemented, however it was less complete (elegent/terse) and I'll need more time/brain power to settle it into something I'm happy with.
 - I'm aware that while the Validate functionality is nice to have, it doesn't give good or detailed information on how to fix any problems. Unfortunately I'm not currently capable of solving this problem, but I intend to revisit it once I'm recovered.

## The API
--------------------
### esd namespace functions
These are the functions called when using the library, and are in the "esd" namespace.
````C++
/**
 * Will return true only if the specified JSON structure can succesfully be deserialised 
 * into type T by calling esd::Deserialise
 */
template <typename T> requires TypeSupportedByEasySerDes<T>
bool Validate(const nlohmann::json& serialised);

template <typename T> requires TypeSupportedByEasySerDes<T>
nlohmann::json Serialise(const T& value);

template <typename T> requires TypeSupportedByEasySerDes<T>
std::optional<T> Deserialise(const nlohmann::json& serialised);

template <typename T> requires TypeSupportedByEasySerDes<T>
T DeserialiseWithoutChecks(const nlohmann::json& serialised);
````
### 
The library uses template specialisation and type erasure to support any type

### HelperType
These are the functions you will use when writing the boilerplate required to support your own types

    [[nodiscard]] T Deserialise(const nlohmann::json& toDeserialise)

    template <typename ReturnType>
    [[nodiscard]] ReturnType Deserialise(const std::function<ReturnType (ConstructionArgs...)>& factory, const std::function<T& (ReturnType&)>& accessResultFromFactoryOutput, const nlohmann::json& toDeserialise)
    {
        size_t index = 0;
        ReturnType retVal =  std::invoke(factory, esd::DeserialiseWithoutChecks<ConstructionArgs>(toDeserialise.at(constructionVariables_.at(index++)))...);
        T& deserialised = std::invoke(accessResultFromFactoryOutput, retVal);
        for (auto& initialiser : initialisationCalls_) {
            std::invoke(initialiser, toDeserialise, deserialised);
        }
        for (const auto& [ key, memberHelper ] : variables_) {
            memberHelper.parser_(toDeserialise.at(key), deserialised);
        }
        std::invoke(postDeserialisationAction_, toDeserialise, deserialised);
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

    void RegisterConstruction(Parameter<ConstructionArgs>... params)
    {
        constructor_ = [=](const nlohmann::json& serialised) -> T
        {
            return { (esd::DeserialiseWithoutChecks<ConstructionArgs>(serialised.at(params.parameterKey_)))... };
        };

        (this->constructionVariables_.push_back(params.parameterKey_), ...);
    }

    /*
     * Specialisation to allow the user to define a validator that can cross
     * reference the construction parameters, useful to prevent crashes from
     * invalid construction.
     */

    template <typename Validator>
    requires std::is_invocable_r_v<bool, Validator, const ConstructionArgs&...>
    void RegisterConstruction(Validator&& parameterValidator, Parameter<ConstructionArgs>... params)
    {
        interdependantVariablesValidators_.push_back([=, validator = std::move(parameterValidator)](const nlohmann::json& serialised) -> bool
        {
            return std::invoke(validator, (esd::DeserialiseWithoutChecks<ConstructionArgs>(serialised.at(params.parameterKey_)))...);
        });

        RegisterConstruction(std::move(params)...);
    }

    // TODO need a definition here that accepts a factory function

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
    [[ nodiscard ]] Parameter<VariableTypeFromInvocable_t<Invocable, const T&>> CreateParameter(Invocable&& valueGetter, std::optional<std::string>&& label = std::nullopt, std::optional<std::function<bool(const VariableTypeFromInvocable_t<Invocable, const T&>&)>>&& customValidator = std::nullopt)
    {
        using ParamType = VariableTypeFromInvocable_t<Invocable, const T&>;
        return RegisterVariableAsParameter<Invocable, ParamType>(std::move(valueGetter), std::move(label), std::move(customValidator));
    }

    ///
    /// Overload for function pointers/lambdas that accept no parameters
    ///
    template <class Invocable>
    requires std::is_invocable_v<Invocable>
    [[ nodiscard ]] Parameter<VariableTypeFromInvocable_t<Invocable>> CreateParameter(Invocable&& valueGetter, std::optional<std::string>&& label = std::nullopt, std::optional<std::function<bool(const VariableTypeFromInvocable_t<Invocable>&)>>&& customValidator = std::nullopt)
    {
        using ParamType = VariableTypeFromInvocable_t<Invocable>;
        return RegisterVariableAsParameter<Invocable, ParamType>(std::move(valueGetter), std::move(label), std::move(customValidator));
    }

    ///
    /// Overload for when the user just wants to specify a magic value
    ///
    template <typename ParamType>
    [[ nodiscard ]] Parameter<ParamType> CreateParameter(const ParamType& constValue, std::optional<std::string>&& label = std::nullopt)
    {
        // TODO the custom validator adds an unecessary copyable requirement to ParamType (isn't this a requirement anyway?...)
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
    void RegisterVariable(MemberObjectPointer getterAndSetter, std::optional<std::string>&& label = std::nullopt, std::optional<std::function<bool(const VariableTypeFromInvocable_t<MemberObjectPointer, T&>&)>>&& customValidator = std::nullopt)
    {
        using ParamType = VariableTypeFromInvocable_t<MemberObjectPointer, T&>;

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
          && std::is_invocable_v<Setter, T&, const VariableTypeFromInvocable_t<Getter, const T&>&>
    void RegisterVariable(Getter&& getter, Setter&& setter, std::optional<std::string>&& label = std::nullopt, std::optional<std::function<bool(const VariableTypeFromInvocable_t<Getter, const T&>&)>>&& customValidator = std::nullopt)
    {
        using ParamType = VariableTypeFromInvocable_t<Getter, const T&>;

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
          && std::is_invocable_v<Setter, T&, const VariableTypeFromInvocable_t<Getter>&>
    void RegisterVariable(Getter&& getter, Setter&& setter, std::optional<std::string>&& label = std::nullopt, std::optional<std::function<bool(const VariableTypeFromInvocable_t<Getter>&)>>&& customValidator = std::nullopt)
    {
        using ParamType = VariableTypeFromInvocable_t<Getter>;

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

Adding it to your own project
-----------------------------
To include this in your own CMAKE project as a statically linked library:
```` CMAKE
# Add this file to your project directory, named "CppEasySerDes.cmake"
FetchContent_Declare(
    CppEasySerDes
    GIT_REPOSITORY  https://github.com/SebastianTroy/CppEasySerDes
    GIT_TAG         origin/main
)

set(CPP_EASY_SERDES_BuildTests OFF CACHE INTERNAL "")
set(CPP_EASY_SERDES_RunTests OFF CACHE INTERNAL "")

FetchContent_MakeAvailable(CppEasySerDes)

include_directories(
    "${CppEasySerDes_SOURCE_DIR}"
)
````

```` CMAKE
# Then in your project CMakeLists.txt add
include(FetchContent)
include(CppEasySerDes)

target_link_libraries(<InsertApplicationNameHere>
    PRIVATE
    CppEasySerDes
)
````
TODO
----
 - [ ] Add to website and link to website in README
 - [ ] Support polymorphic types
 - [ ] Support taking a Factory parameter in RegisterConstruction
 - [ ] Make it possible to deserialise types in place via factory functions
 - [ ] POD helper to further reduce boilerplate code
