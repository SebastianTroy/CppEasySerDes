# CppEasySerDes
---------------

A library for easy conversion of C++ types to/from JSON, allowing for a concise single definition that handles serialisation, deserialisation and validation. 

## Table of Contents:
---------------------

 - [Disclaimer](#Disclaimer)
 - [Why?](#Why)
 - [Known Limitations](#Known-Limitations)
 - [Adding It To Your Own Project](#Adding-It-To-Your-Own-Project).
 - [Examples](#Examples)
   - [Using the Library](#Using-the-Library) 
   - [Supporting A Trivial Type](#Supporting-A-Trivial-Type)
   - [Supporting A Complex Type](#Supporting-A-Complex-Type)
   - [Supporting Polymorphic Types](#Supporting-Polymorphic-Types)
 - [The API](#The-API)
   - [esd namespace Functions](#esd-namespace-Functions)
   - [esd::JsonSerialiser](#esdJsonSerialiser)
   - [esd::JsonClassSerialiser](#esdJsonClassSerialiser)
     - [RegisterConstruction](#esdJsonClassSerialiserRegisterConstruction)
     - [RegisterInitialisation](#esdJsonClassSerialiserRegisterInitialisation)
     - [CreateParameter](#esdJsonClassSerialiserCreateParameter)
     - [RegisterVariable](#esdJsonClassSerialiserRegisterVariable)
     - [DefinePostSerialiseAction](#esdJsonClassSerialiserDefinePostSerialiseAction)
     - [DefinePostDeserialiseAction](#esdJsonClassSerialiserDefinePostDeserialiseAction)
   - [esd::JsonPolymorphicClassSerialiser](#esdJsonPolymorphicClassSerialiser)
     - [RegisterChildTypes](#esdJsonPolymorphicClassSerialiserRegisterChildTypes)
 - [TODO](#TODO)

## Disclaimer
-------------

This isn't designed to be fast, it is designed to be convinient, terse, and have human readable output (i.e. JSON).
I'm currently suffering from pretty severe long covid brain fog.
I hashed out the vast majority of the pre-covid and have been using the polishing of this project to gauge my recovery.
I'm not a proud person, if I've made wildly inaccurate claims, or my implementation is dumb, or I've missed something obvious, I'd be thrilled to know! =]

[Back to Index](#Table-of-Contents)

## Why?
-------

nlohmann::json already has its own support for custom types... why re-invent the wheel in a perfectly servicable wheel?
 - A lot of solutions for this problem require you define two functions, a reader and a writer. These are often identical functions but in reverse and must be maintained in tandem, violating the DRY principle.
 - Adding validation or constraints to the acceptable values stored sucks because you need to add the same checks to both the writer and the reader functions.
 - When using a storage structure like JSON, you must now either create variables for each key, so they an be shared between the reader/writer functions, or carefully use the same magic values in both.
 - And now you want to deserialise it into a smart pointer without copying, or emplace_back into an array? Well you'll have to duplicate your deserialisation function!

[Back to Index](#Table-of-Contents)

## Known Limitations
--------------------

- The Validate functionality is nice to have, but it doesn't give detailed (or any) information on how to fix any problems. Unfortunately I'm not currently capable of solving this problem, but I intend to revisit it once I'm recovered from long COVID.
- `HelperType` is a useful typedef but may need to be written out in full if T itself is templated, `InternalHelper<T<...>, ConstructionArgs...>`
- Raw pointers and references are not supported, polymorphic behaviour is only supported via smart pointers

[Back to Index](#Table-of-Contents)

## Adding It To Your Own Project
--------------------------------

To include this in your own CMAKE project as a statically linked library
Create a file named "CppEasySerDes.cmake"in your project directory, containing the follwoing
````CMAKE
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
Then in your project CMakeLists.txt add
````CMAKE
include(FetchContent)
include(CppEasySerDes)

target_link_libraries(<InsertYourProjectNameHere>
    PRIVATE
    CppEasySerDes
)
````

[Back to Index](#Table-of-Contents)

## Examples
-----------

### Using the Library

A theoretical save game system may look like the following:
````C++
void SaveGame(const GameState& state)
{
    std::ofstream file;
    file.open("save-game.txt");
    file << std::setw(4) << esd::Serialise(state);
}
````
To load the game state you have two options, the following uses std::optional and offloads the logic required to check if it was succesful to the caller.
````C++
std::optional<GameState> LoadGame(const std::string& filename)
{
    std::ifstream file(filename);
    nlohmann::json serialisedGameState;
    file >> serialisedGameState;
    return esd::Deserialise<GameState>(serialisedGameState);
}
````
The other option is to do the checks ourselves
````C++
void LoadGame(const std::string& filename)
{
    std::ifstream file(filename);
    nlohmann::json serialisedGameState;
    file >> serialisedGameState;
    if (esd::Validate<GameState>(serialisedGameState)) {
        LoadGameState(esd::DeserialiseWithoutChecks<GameState>(serialisedGameState));
    } else {
        std::cout << "Failed to load save from file: " << filename << std::endl;
    }
}
````

### Supporting A Trivial Type
-----------------

This is intended to be the most common use case. The following example demonstrates how you would use the esd::JsonClassSerialiser to support a struct, and optionally assign a label and/or additional validation to each struct member variable.
````C++
struct TrivialType {
    int a_;
    std::vector<int> b_;
    std::string c_;
};

template<>
struct esd::JsonSerialiser<TrivialTestType> : public esd::JsonClassSerialiser<TrivialTestType, int, std::vector<int>, std::string> {
    static void SetupHelper(HelperType& h)
    {
        // Here we pass in static member object pointers that can be applied to an instance of the type later
        h.RegisterConstruction(h.CreateParameter(&TrivialTestType::a_), 
                               h.CreateParameter(&TrivialTestType::b_, "B"),
                               h.CreateParameter(&TrivialTestType::c_, std::nullopt, [](const std::string&) -> bool { /* custom validation */ }));
    }
};
````

[Back to Index](#Table-of-Contents)

### Supporting A Complex Type
-------------------------

A contrived example to show off more features. 
````C++
// Class definitions not included for brevity
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
    const std::string& GetD_C() const;

private:
    int b_;
    double c_;
    TrivialType d_;
};

template<>
class esd::JsonSerialiser<ContrivedType> : public esd::JsonClassSerialiser<ContrivedType, int> {
public:
    static void SetupHelper(HelperType& h)
    {
        // b_ is private, so instead we use a static member function pointer to its getter
        h.RegisterConstruction(h.CreateParameter(&ContrivedType::GetB));

        // First we specify what the initialisation call is, then we specify the parameters the same way we do for the constructor
        h.RegisterInitialisation(&ContrivedType::Initialise, h.CreateParameter(&ContrivedType::a_), h.CreateParameter(&ContrivedType::GetC));

        // Custom Getter or Setters can be provided for any variable or parameter, not only member pointers are supported
        auto customGetD = [](const ContrivedType& toSerialise) -> TrivialType
                          {
                              return { toSerialise.GetD_A(), toSerialise.GetD_B(), toSerialise.GetD_C() };
                          };
        // Register a variable that isn't covered in construction or via an initialiser (note that CreateParameter calls aren't necessary or supported here)
        h.RegisterVariable(std::move(customGetD), &ContrivedType::SetD);

        /*
         * The following two functions allow complete flexibility in how you serialise and 
         * deserialise your type, they should rarely be necesary if ever. Perhaps consider
         * if serialising a type that requires these is actually the correct strategy!
         */

        // Perhaps add something custom to the json, or do global housekeeping
        h.DefinePostSerialiseAction([](const ContrivedType& input, nlohmann::json& output){ /* ... */ });
        // Perhaps parse something custom from the json, or further validate the output, or do global housekeeping
        h.DefinePostDeserialiseAction([](const nlohmann::json& input, ContrivedType& output){ /* ... */ });
    }
};
````

[Back to Index](#Table-of-Contents)

## Supporting Polymorphic Types
-------------------------------

And finally some examples showing how to support polymorphism.
First some polymorphic types to support:

````C++
class ParentType {
public:
    int a_;
    virtual ~ParentType() {}
};
````
````C++
class ChildTypeA : public ParentType {
public:
    ChildTypeA(bool b);
    virtual ~ChildTypeA() {}
    
    bool GetB() const;
    
private:
    bool b_;
};
````
````C++
class ChildTypeB : public ParentType {
public:
    ChildTypeB(std::string s);
    virtual ~ChildTypeB() {}

    std::string GetS() const;
    
private:
    std::string s_;
};
````
````C++
class GrandChildType : public ChildTypeA {
public:
    int i_;
    GrandChildType(int i);
    virtual ~GrandChildType() {}
};
````

Next we need to implement their `JsonSerialiser`s. Note that they are implemented in reverse order.

````C++
template<>
class esd::JsonSerialiser<GrandChildType> : public esd::JsonPolymorphicClassSerialiser<GrandChildType, int> {
public:
    static void SetupHelper(HelperType& h)
    {
        h.RegisterConstruction(h.CreateParameter(&GrandChildType::i_));
    }
};

template<>
class esd::JsonSerialiser<ChildTypeA> : public esd::JsonPolymorphicClassSerialiser<ChildTypeA, bool> {
public:
    static void SetupHelper(HelperType& h)
    {
        h.RegisterConstruction(h.CreateParameter(&ChildTypeA::GetB));
        RegisterChildTypes<GrandChildType>();
    }
};

template<>
class esd::JsonSerialiser<ChildTypeB> : public esd::JsonPolymorphicClassSerialiser<ChildTypeB, std::string> {
public:
    static void SetupHelper(HelperType& h)
    {
        h.RegisterConstruction(h.CreateParameter(&ChildTypeB::GetS));
    }
};

template<>
class esd::JsonSerialiser<ParentType> : public esd::JsonPolymorphicClassSerialiser<ParentType, int> {
public:
    static void SetupHelper(HelperType& h)
    {
        h.RegisterConstruction(h.CreateParameter(&ParentType::a_));
        RegisterChildTypes<ChildTypeA, ChildTypeB>();

        // As ParentType is default constructable, We could also have specified 
        // no construction params, not registered the construction and instead
        // called `h.RegisterVariable(&ParentType::a_);`
    }
};
````

And finally, to Serialise and Deserialise these types, you have three options:
Firstly, when using the types directly, you have to be careful of slicing:

````C++
ChildTypeA childInstance{ 42 };
nlohmann::json serialised = esd::Serialise(p);
// Uhoh, we've just sliced. Our parentInstance vtable will be invalid
ParentType parentInstance = esd::DeserialiseWithoutChecks<ChildTypeA>(serialised);
````

The following wont work because they are trying to reference/point at a tempory

````C++
ParentType& parentInstance = esd::DeserialiseWithoutChecks<ChildTypeA>(serialised);
ParentType* parentInstance = &esd::DeserialiseWithoutChecks<ChildTypeA>(serialised);
````

The following won't work because we haven't defined `JsonSerialiser<ChildTypeA&>` or `JsonSerialiser<ChildTypeA*>` (nor should we!)

````C++
ParentType& parentInstance = esd::DeserialiseWithoutChecks<ChildTypeA&>(serialised);
ParentType* parentInstance = esd::DeserialiseWithoutChecks<ChildTypeA*>(serialised);
````

Secondly, the intended usage is via smart pointers

````C++
std::shared_ptr<ParentType> originalSharedInstance = std::make_shared<GrandChildType>(79);
nlohmann::json serialised = esd::Serialise(originalSharedInstance);
// This DOES NOT point to the same instance as originalSharedInstance
std::shared_ptr<ParentType> deserialisedSharedInstance = esd::DeserialiseWithoutChecks<std::shared_ptr<ParentType>>(serialised);
// This DOES point to the same instance as deserialisedSharedInstance
std::shared_ptr<ParentType> anotherSharedInstance = esd::DeserialiseWithoutChecks<std::shared_ptr<ParentType>>(serialised);
// This DOES NOT point to the same instance as deserialisedSharedInstance
esd::CacheManager::ClearCaches();
std::shared_ptr<ParentType> yetAnotherDeserialisedSharedInstance = esd::DeserialiseWithoutChecks<std::shared_ptr<ParentType>>(serialised);

std::unique_ptr<ParentType> originalUniqueInstance = std::make_unique<GrandChildType>(79);
nlohmann::json serialised = esd::Serialise(originalUniqueInstance);
std::unique_ptr<ParentType> deserialisedUniqueInstance = esd::DeserialiseWithoutChecks<std::unique_ptr<ParentType>>(serialised);
````

Something to note is that the serialised forms of `unique_ptr<T>` and `shared_ptr<T>` are not compatable

````C++
std::unique_ptr<ParentType> originalUniqueInstance = std::make_unique<GrandChildType>(79);
nlohmann::json serialisedUniquePtr = esd::Serialise(originalUniqueInstance);
// Evaluates to false
esd::Validate<std::shared_ptr<ParentType>>(serialisedUniquePtr);
// Terminates execution
// std::shared_ptr<ParentType> deserialisedUniqueInstance = esd::DeserialiseWithoutChecks<std::shared_ptr<ParentType>>(serialised);
// Works because a shared_ptr can be constructed/assigned from a unique_ptr
std::shared_ptr<ParentType> deserialisedUniqueInstance = esd::DeserialiseWithoutChecks<std::unique_ptr<ParentType>>(serialised);
````

Thirdly, via the esd::JsonPolymorphicClassHelper ValidatePolymorphic, SerialisePolymorphic, and DeserialisePolymorphic functions (The same would work with unique_ptr)

````C++
std::shared_ptr<ParentType> originalSharedInstance = std::make_shared<GrandChildType>(79);
nlohmann::json serialised = esd::Serialise(originalSharedInstance);
// This DOES NOT point to the same instance as originalSharedInstance
std::shared_ptr<ParentType> deserialisedSharedInstance = esd::JsonPolymorphicClassSerialiser<BaseTestType, int>::DeserialisePolymorphic(serialised);
// This DOES NOT point to the same instance as deserialisedSharedInstance (the shared_ptr tracking is done inside the `JsonSerialiser<std::shared_ptr<...>>` specialisation)
std::shared_ptr<ParentType> anotherSharedInstance = esd::JsonPolymorphicClassSerialiser<BaseTestType, int>::DeserialisePolymorphic(serialised);
````

[Back to Index](#Table-of-Contents)

## The API
----------

### esd namespace Functions
---------------------------

These are the functions called when using the library, and are in the "esd" namespace.

##### Validate
--------------

Will return true only if the specified JSON structure can succesfully be deserialised into type T by calling esd::Deserialise<T>(serialised)
````C++
template <typename T> requires TypeSupportedByEasySerDes<T>
bool Validate(const nlohmann::json& serialised);
````

[Back to Index](#Table-of-Contents)

##### Serialise
---------------

Will return a JSON structure that can succesfully be deserialised into type T by calling `esd::DeserialiseWithoutChecks<T>`, and a call to `esd::Validate<T>(esd::Serialise<T>(...))` will return true.
````C++
template <typename T> requires TypeSupportedByEasySerDes<T>
nlohmann::json Serialise(const T& value);
````

[Back to Index](#Table-of-Contents)

#### Deserialise
----------------

 If `esd::Validate<T>(serialised)` returns true, the returned optional will contain a valid instance of type T, else it will be equal to `std::nullopt`.
````C++
template <typename T> requires TypeSupportedByEasySerDes<T>
std::optional<T> Deserialise(const nlohmann::json& serialised);
````

[Back to Index](#Table-of-Contents)

#### DeserialiseWithoutChecks
-----------------------------

`Validate<T>(serialised)` is not called, and should be first called by the user. If the json cannot be succesfully converted into an instance of type T the program will halt. This is useful for cases where the end type cannot easily be extracted from a `std::optional<T>`.
````C++
template <typename T> requires TypeSupportedByEasySerDes<T>
T DeserialiseWithoutChecks(const nlohmann::json& serialised);
````

[Back to Index](#Table-of-Contents)

### esd::JsonSerialiser
-----------------------

For any of your types to be supported by this library, you must create a specialisation of the following templated type.
Every esd::JsonSerialiser must implement a static Validate, Serialise and Deserialise function.
It is **NOT RECOMMENDED** that you support your types directly like this, see esd::JsonClassSerialiser and esd::JsonPolymorphicClassSerialiser below!
````C++
template<>
class esd::JsonSerialiser<T> {
public:
    static bool Validate(const nlohmann::json& serialised);
    static nlohmann::json Serialise(const T& value);
    static T Deserialise(const nlohmann::json& serialised);
};
````

[Back to Index](#Table-of-Contents)

### esd::JsonClassSerialiser
----------------------------

To make use of the `JsonClassSerialiser` you publically extend it when you are implementing an esd::JsonSerialiser.
It already defines the Validate, Serialise and Deserialise functions for you, so all that remains is to define a static SetupHelper function as below.
````C++
template<>
class esd::JsonSerialiser<T> : public esd::JsonClassSerialiser<T, ConstructionParameterTypes...> {
public:
    static void SetupHelper(HelperType& h);
};
````
#### esd::JsonClassSerialiser::RegisterConstruction
---------------------------------------------------

This **MUST** be called if your type is not default constructable, or if your type is default constructable and has an alternative constructor that you would like to use.
**Repeated calls to this function simply overwrite previous calls.**
`RegisterConstruction` takes a series of `Parameter`s which are created by the helper.
The actual type of `Parameter` is private in this context, so a Parameter has to be created in place within the call to `RegisterConstruction`.

````C++
static void SetupHelper(HelperType& h)
{
    h.RegisterConstruction(h.CreateParameter(...), ...);
}
````
`CreateParameter` is a template, and while the type will be **automatically deduced**, it is important that the deduced types match the target types construction signature, ignoring constness, references and r values e.t.c.
If the constructor takes int and const string reference
````C++
T(int a, const std::string& b);
````
The the parameters must be equivalent to the follwoing
````C++
h.RegisterConstruction(h.CreateParameter<int>(...), h.CreateParameter<std::string>(...));
````
While each parameter may have its own individual validation, in some cases it may be desirable to check the validity of the parameters relative to each other,
this can be important to prevent program crashes when constructing a type with invalid arguments that individually evaluated as valid values.
Note that the validation function must be callable with the same parameters as the constructor of the target type.
Again whether the types are const, references or r-values is irrelevent, and they *can* be different to the construction signature.
````C++
bool ValidateFunc(int i, const std::string& s)
{
    return i < s.size();
}
````
````C++
// Defer to an existing function
h.RegisterConstruction(&ValidateFunc, h.CreateParameter<int>(...), h.CreateParameter<std::string>(...));
// OR use a lambda
h.RegisterConstruction([](int i, const std::string& s){ return i < s.size(); }, h.CreateParameter<int>(...), h.CreateParameter<std::string>(...));
````

[Back to Index](#Table-of-Contents)

#### esd::JsonClassSerialiser::RegisterInitialisation
-----------------------------------------------------

Some types may not complete their initialisation within a constructor, but instead require a subsequent call to some initialise function.
As many initialise calls can be registered as necessary.
`RegisterInitialisation` takes a member function pointer, to a public member function of the target type, and a series of `Parameter`s which are created by the helper.
The actual type of `Parameter` is private in this context, so a Parameter has to be created in place within the call to `RegisterInitialisation`.

````C++
class T {
public:
    Initialise(bool b, const std::string& s);
};
````
````C++
static void SetupHelper(HelperType& h)
{
    h.RegisterInitialisation(&T::Initialise, h.CreateParameter(...), h.CreateParameter(...));
}
````
`CreateParameter` is a template, and while the type will be **automatically deduced**, it is important that the deduced types match the provided initialise function's signature, ignoring constness, references and r values e.t.c.
In the above examplethe parameters must be equivalent to the follwoing
````C++
h.RegisterInitialisation(h.CreateParameter<bool>(...), h.CreateParameter<std::string>(...));
````
While each parameter may have its own individual validation, in some cases it may be desirable to check the validity of the parameters relative to each other,
this can be important to prevent program crashes due to invalid parameters that individually evaluated as valid values.
Note that the validation function must be callable with the same parameters as the initialise function.
Again whether the types are const, references or r-values is irrelevent, and they *can* be different to the construction signature.
````C++
bool ValidateFunc(bool b, const std::string& s)
{
    return b == s.empty();
}
````
````C++
// Defer to an existing function
h.RegisterInitialisation(&ValidateFunc, h.CreateParameter<bool>(...), h.CreateParameter<std::string>(...));
// OR use a lambda
h.RegisterInitialisation([](bool b, const std::string& s){ return b == s.empty(); }, h.CreateParameter<bool>(...), h.CreateParameter<std::string>(...));
````

[Back to Index](#Table-of-Contents)

#### esd::JsonClassSerialiser::CreateParameter
----------------------------------------------

This function is used with both `RegisterConstruction` and `RegisterInitialisation`, as both require `0` or more `CreateParameter` return types as input.
The role of this function is to tell the library how to get the values needed to construct our type and call any initialise functions.

The `getter` parameter of the first overload can be:
|                 Name                    |                   Example                   |
| --------------------------------------- | ------------------------------------------- |
| A public object member pointer          | `&T::memberVariableName_`                   |
| A public object member function pointer | `&T::MemberFuncName`                        |
| A std::function                         | `std::function<ParamType(const T&)>{ ... }` |
| A lambda                                | `[](const T&) -> ParamType { ... }`         |

Note they can all be invoked with a single `const T&` parameter, to return a value of `ParamType`.
This is the most likely use case, as you are using information from an instance of type `T` to save and load instances of type `T`
````C++
CreateParameter(function<ParamType(const T&)> getter, optional<string> label, optional<function<bool(const ParamType&)>> validator);
````

-----

The `getter` parameter of the second overload can be:
|      Name       |               Example               |
| --------------- | ----------------------------------- |
| A std::function | `std::function<ParamType()>{ ... }` |
| A lambda        | `[]() -> ParamType { ... }`         |

Note they can all be invoked with no parameters, to return a value of `ParamType`.
This overload is to be used in cases where a type `T` is constructed from values that are not obtainable from the class itself. Perhaps values that are obtainable from a config file e.t.c. This overload should not be required often.

````C++
CreateParameter(function<ParamType()> getter, optional<string> label, optional<function<bool(const ParamType&)>> validator);
````

-----

The `value` parameter of the third overload must be a value that can be copied:
|      Name       |                     Examples                     |
| --------------- | ------------------------------------------------ |
| Magic value     | `42`, `"Foo"`, `true`                            |
| Constexpr value | `sizeof(UINT8)`, `numeric_limits<int>::digits10` |
| Static Variable | T::STATIC_VALUE                                  |
| Global Variable | `SOME_#DEFINE`, `SOME_GLOBAL_VARIABLE`           |

This overload should not be required often.

````C++
CreateParameter(const ParamType& value, optional<string> label);
````

-----

The `label` and `validator` parameters use `std::optional` and are defaulted to `std::nullopt`.
You do not need to specify a `std::optional` for either, e.g. simply pass a `std::string` for `label` and the `std::optional<std::string>` will be automatically constructed during the function call.

The `label` is useful for debugging, and may be present in the serialised data, e.g. used as keys in JSON.
The `label` parameter in each overload is optional. If a label is specified it should be unique, and duplicate labels will be modified to ensure uniqueness.
You can specify nothing, `{}` or `std::nullopt` to ignore the label, but a unique label will be generated automatically.

The `validator` allows you to add custom validation to the value, e.g. you could require an int be limited between `0` and `10`.
You can specify nothing, `{}` or `std::nullopt` and no additional validation will occur, however the base validation always occurs (i.e. that the stored value can be converted to the correct type).

Some usage examples:
````C++
CreateParam(42);
CreateParam(&T::min, "min");
CreateParam(&T::GetEmail, std::nullopt, [emailRegex](std::string&& email) -> bool { std::regex_match(email, emailRegex); });
CreateParam([](const T& instance) -> std::pair<int, int> { return std::make_pair(instance.Min(), instance.Max()); }, "min-max");
````

[Back to Index](#Table-of-Contents)

#### esd::JsonClassSerialiser::RegisterVariable
----------------------------------------------

Some types may have member variables that are not set via a constructor or initialise function, and therefore need to be dealt with seperately.
There are a few overloads, which are similar to `CreateParameter`, except that we also need to specify a `setter` as well as the `getter`.

In the case where the variable is a public member of the type, `&T::memberVariableName_` can be used for both reading and writing the value.

````C++
RegisterVariable(MemberObjectPointer* getterAndSetter, optional<string> label, optional<bool(const ParamType&)> validator)
````

The `getter` parameter of the second overload can be:
|                  Name                   |                   Example                   |
| --------------------------------------- | ------------------------------------------- |
| (*A public object member pointer*)      | `&T::memberVariableName_`                   |
| A public object member function pointer | `&T::MemberFuncName`                        |
| A std::function                         | `std::function<ParamType(const T&)>{ ... }` |
| A lambda                                | `[](const T&) -> ParamType { ... }`         |

Note they can all be invoked with a single `const T&` parameter, to return a value of `ParamType`. Consider using the first overload if you have access to a member object pointer.

````C++
RegisterVariable(function<ParamType(const T&)> getter, function<void(T&, const ParamType&)> setter, optional<string> label, optional<bool(const ParamType&)> validator)
````

The `getter` parameter of the third overload can be:
|                  Name                   |               Example               |
| --------------------------------------- | ----------------------------------- |
| A std::function                         | `std::function<ParamType()>{ ... }` |
| A lambda                                | `[]() -> ParamType { ... }`         |

Note they can all be invoked with no parameters, to return a value of `ParamType`.

````C++
RegisterVariable(function<ParamType()> getter, function<void(T&, const ParamType&)> setter, optional<string> label, optional<bool(const ParamType&)> validator)
````

The `setter` parameter is the same in both the second and third overloads, and can be:
|                   Name                  |                             Example                             |
| --------------------------------------- | --------------------------------------------------------------- |
| A std::function                         | `std::function<void(T& target, const ParamType& value)>{ ... }` |
| A lambda                                | `[](T& target, const ParamType& value) -> void { ... }`         |
| A public object member function pointer | `&T::SetX`                                                      |

Where the lambda and ember object pointers will automatically be wrapped in a std::function.

The `label` and `validator` parameters use `std::optional` and are defaulted to `std::nullopt`.
You do not need to specify a `std::optional` for either, e.g. simply pass a `std::string` for `label` and the `std::optional<std::string>` will be automatically constructed during the function call.

The `label` is useful for debugging, and may be present in the serialised data, e.g. used as keys in JSON.
The `label` parameter in each overload is optional. If a label is specified it should be unique, and duplicate labels will be modified to ensure uniqueness.
You can specify nothing, `{}` or `std::nullopt` to ignore the label, but a unique label will be generated automatically.

The `validator` allows you to add custom validation to the value, e.g. you could require an int be limited between `0` and `10`.
You can specify nothing, `{}` or `std::nullopt` and no additional validation will occur, however the base validation always occurs (i.e. that the stored value can be converted to the correct type).

Some usage examples:
````C++
RegisterVariable(&T::count, "Count");
RegisterVariable(&T::GetEmail, &T::SetEmail, std::nullopt, [emailRegex](std::string&& email) -> bool { std::regex_match(email, emailRegex); });

auto getter = [](const T& instance) -> std::pair<int, int> 
              { 
                  return std::make_pair(instance.Min(), instance.Max()); 
              };
auto setter = [](T& target, const std::pair<int, int>& minMax)
              {
                  target.SetMinMax(minMax);
              };
auto validdator = [](const std::pair<int, int>& minMax) -> bool
                  {
                     return minMax.first <= minMax.second;
                  };
RegisterVariable(std::move(getter), std::move(setter), "min-max", std::move(validator));
````

[Back to Index](#Table-of-Contents)

#### esd::JsonClassSerialiser::DefinePostSerialiseAction
--------------------------------------------------------

This function is included alongside `DefinePostDeserialiseAction` to allow complete flexibility. It is not intended that these functions be used often.

This function provides complete access to the generated output just before it is serialised, allowing you to add any additional data you wish to save, and you can then read that data in an equal but opposite implementation passed to the `DefinePostDeserialiseAction` function. Again, this is not recommended.

You can also ignore the input parameters and simply use the function to perform some global book-keeping e.c.t.

````C++
void DefinePostSerialiseAction(std::function<void (const T&, nlohmann::json&)>&& action);
````

[Back to Index](#Table-of-Contents)

#### esd::JsonClassSerialiser::DefinePostDeserialiseAction
----------------------------------------------------------

This function is included alongside `DefinePostSerialiseAction` to allow complete flexibility. It is not intended that these functions be used often.

This function provides complete access to an instance of `T` just after it is has been deserialised, allowing you to modify it in any way you desire, and you have access to parsed data allowing you to read any additional items you stored in your implementation passed to `DefinePostSerialiseAction`. Again, this is not recommended.

You can also ignore the input parameters and simply use the function to perform some global book-keeping e.c.t.

````C++
void DefinePostDeserialiseAction(std::function<void (const nlohmann::json&, T&)>&& action);
````

[Back to Index](#Table-of-Contents)

### esd::JsonPolymorphicClassSerialiser
---------------------------------------

To make use of the `JsonPolymorphicClassSerialiser` you publically extend it when you are implementing an esd::JsonSerialiser.
It it an extension of `JsonClassSerialiser` so it already defines the Validate, Serialise and Deserialise functions for you, and you implement the same static SetupHelper function as below.
````C++
template<>
class esd::JsonSerialiser<T> : public esd::JsonPolymorphicClassSerialiser<T, ConstructionParameterTypes...> {
public:
    static void SetupHelper(HelperType& h);
};
````

#### esd::JsonPolymorphicClassSerialiser::RegisterChildTypes
------------------------------------------------------------

The only difference between using this type and `esd::JsonClassSerialiser` is that you need to specify the child types of your type.

````C++
// Call this in `SetupHelper`, it is not a member of `HelperType`
RegisterChildTypes<ChildT1, CHildT2, CHildT3, ...>();
````

Each child type must also have a defined `esd::JsonSerialiser<ChildT>` that extends `esd::JsonPolymorphicClassSerialiser`. The requirement to specify the child types means the definitions of each serialiser must be defined in reverse heirarchical order, i.e. from grandest child type first, to base type last.

[Back to Index](#Table-of-Contents)

## TODO
-------

 [x] Add tests
 [x] Support std library types
 [x] Support Polymorphism
 [x] Adjust the content of this README to match the table of content
 [ ] Inline Documentation of why I've created various types and functions, with intended purpose
 [ ] Hide HelperType from the user, implement static passthrough methods inside `esd::JsonClassHelper` itself, hide the `HelperType` from the user, and change `void SetupHelper(HelperType h)` to `void Configure()`
 [ ] Create an `esd` directory and move all headers into it, renaming them to remove the `EasySerDes` prefix (except EasySerDes.h!)
 [ ] Remove Json prefixes from type and header names
 [ ] Change type names to match header names
 [ ] Refactor `RegisterConstruction` to `SetConstructor` so it is clear it should only be called once
 [ ] Refactor `RegisterInitialisation` to `AddInitialisationCall` so it is clear it can be called multiple times
 [ ] Refactor `RegisterChildTypes` to `SetChildTypes` so it is clear it should be called only once
 [ ] Add to website and link to website in README
 [ ] In the "Adding It To Your Own Project" section, link to one of my projects that uses this library as a complete example
 [ ] MAYBE POD helper (that would implement the `SetupHelper` function automatically)
 [ ] MAYBE Enum helper to allow setting of stricter validation of values
 [ ] MAYBE seperate the `nlohmann::json` requirement from the API, would need an API with some std::library default impl, optional nlohmann impl, and ability for end user to create their own to easily serialise to any format and back, would need to pass as a template param, as it dictates the return type and parameter types of various functions...
 [ ] create an EnumHelper that allows the user to set a max and min value, or set allowed values, or set valid flags etc

[Back to Index](#Table-of-Contents)
