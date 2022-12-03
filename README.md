# CppEasySerDes
---------------

A library for easy conversion of C++ types to/from JSON, allowing for a concise single definition that handles serialisation, deserialisation and validation. 

## Table of Contents:
---------------------

 - [Disclaimer](#Disclaimer)
 - [Why?](#Why)
 - [Known Limitations](#Known-Limitations)
 - [Adding It To Your Own Project](#Adding-It-To-Your-Own-Project).
 - [Types Supported](#Types-Supported)
   - [Built in Types](#Built-in-Types) 
   - [User Types](#User-Types)
   - [Standard Library](#Standard-Library)
   - [`std::ranges::range<T>`](#stdrangesrange)
   - [`shared_ptr` and `unique_ptr`](#shared_ptr-and-unique_ptr)
     - [Polymorphism](#polymorphism) 
     - [`shared_ptr` tracking](#)
 - [Examples](#Examples)
   - [Using the Library](#Using-the-Library) 
   - [Supporting A Trivial Type](#Supporting-A-Trivial-Type)
   - [Supporting A Complex Type](#Supporting-A-Complex-Type)
   - [Supporting Polymorphic Types](#Supporting-Polymorphic-Types)
 - [The API](#The-API)
   - [esd namespace Functions](#esd-namespace-Functions)
   - [esd::Serialiser](#esdSerialiser)
   - [esd::ClassHelper](#esdClassHelper)
     - [SetConstruction](#esdClassHelperSetConstruction)
     - [AddInitialisationCall](#esdClassHelperAddInitialisationCall)
     - [CreateParameter](#esdClassHelperCreateParameter)
     - [RegisterVariable](#esdClassHelperRegisterVariable)
     - [DefinePostSerialiseAction](#esdClassHelperDefinePostSerialiseAction)
     - [DefinePostDeserialiseAction](#esdClassHelperDefinePostDeserialiseAction)
     - [DeserialiseInPlace](#esdClassHelperDeserialiseInPlace)
   - [esd::PolymorphismHelper](#esdPolymorphismHelper)
     - [IsDerivedType](#esdPolymorphismHelperIsPolymorphic)
     - [ValidatePolymorphic](#esdPolymorphismHelperValidatePolymorphic)
     - [SerialisePolymorphic](#esdPolymorphismHelperSerialisePolymorphic)
     - [DeserialisePolymorphic](#esdPolymorphismHelperDeserialisePolymorphic)
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
 - When using a storage structure like JSON, you must now either create variables for each key, so they can be shared between the reader/writer functions, or carefully use the same magic values in both.
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

## Types Supported
------------------

### Built in Types
------------------

Any `enum` or `enum class` you create will be automatically supported, as a numerical values.
All numerical types are supported, even those larger than supported natively by nlohmann::json.
`bool` is supported and stored as `"true"` or `false` strings.
`char` has an overload and is stored as a string containing a single character. (Note that containers of `char` are specialised to store a single string containing all characters, rather than a list of single character strings).

### User Types
--------------

To support classes and structs, simply define an `esd::Serialiser` template overload (See [esd::ClassHelper](#esdClassHelper) for details):

````C++
class esd::Serialiser<MyType> : esd::ClassHelper<MyType, ConstructionArgs...> {
    void Configure()
    {
        ...
    }
};
````

It is also possible to create custom overloads for non-class types (technically you can support classes and structs this way too):

````C++
class esd::Serialiser<T> {
    bool Validate(const nlohmann::json& serialised)
    {
        ...
    }

    nlohmann::json Serialise(const T& instance)
    {
        ...
    }

    T Deserialise(const nlohmann::json& serialised)
    {
        ...
    }
};
````

### Standard Library
--------------------

The intent is for all types to be supported, if you need one that is missing raise a bug report and it will be added.

 - `std::byte`, stored as a four character string e.g. `"0xFF"`. // FIXME this is good for readability bad for storage efficiency, needs to be a user choice
 - `std::pair`, stored as values named `"first"` and `"second"`.
 - `std::tuple`, stored as values named e.g. `std::tuple<int, Foo<T>` `"T0int"` and `"T1Foo<T>"`.
 - `std::string`
 - `std::bitset`, stored as a string containing only `'0'`s and `'1'`s.
 - `std::array`
 - `std::optional`, stored either as the contained value, or a `"std::nullopt"` string.
 - `std::variant`, stored as a list with an entry for each value. One entry will be stored as a value, all others will be a `"nullvariant"` string.
 - `std::unique_ptr`, stored either as the contained value, or a `"nullptr"` string.
 - `std::shared_ptr`, stored with some additional data (see [`shared_ptr` tracking](#shared_ptr-tracking))

### std::ranges::range
----------------------

Any type that satisfies the concept `std::ranges::range<T>` is supported automatically (provided that the range's `value_t` is also supported). It is possible to override this by creating your own specialisation e.g. `class Serialiser<MyRange<T>> ...`.

Notably this supports all standard library containers but doesn't validate for example that all of a set's items, or map's keys are unique, just that all items are individually valid.

There is also a specialisation for ranges of `char`, so that they are stored as a single string, rather than a list of characters.

### `shared_ptr` and `unique_ptr`
-----------------------------

#### Polymorphism
-----------------

Both `shared_ptr` and `unique_ptr` support polymorphic types.

For the following example to work:
 - `DerivedType` is derived from `ParentType`
 - `esd::Serialiser<ParentType` and `esd::Serialiser<DerivedType>` specialisations exist,
 - There is an `esd::PolymorphismHelper<ParentType>` specialisation, which .

See the section on [Supporting Polymorphic Types](#Supporting-Polymorphic-Types) for more details.

````C++
std::unique_ptr<ParentType> basePtr = std::make_unique<DerivedType>(args...);

auto serialised = esd::Serialise(basePtr);

// Even though we're using the `ParentType` Deserialise function, our deserialisedPtr will point to an instance of `DerivedType`
std::unique_ptr<ParentType> deserialisedPtr = esd::DeserialiseWithoutChecks<std::unique_ptr<ParentType>>(serialised);
````

`shared_ptr` tracking
---------------------

`shared_ptr` is a special case, during a call to `esd::Deserialise` it will check the serialised type to see if an existing shared_ptr has already been created by the library during the current call to `esd::Deserialise(...)`.

````C++
std::shared_ptr<T> originalInstance = std::make_shared<T>(args...);

std::vector<decltype(instance)> instances;
for (size_t i = 0; i < 100; ++i) {
    instances.push_back(originalInstance);
    assert(instances[i].get() == originalInstance.get()); // true, they're pointers to the same address
}

auto serialised = esd::Serialise(vectorOfInstances);

std::vector<decltype(instance)> deserialisedInstances = esd::DeserialiseWithoutChecks<std::vector<decltype(instance)>>(serialised);

for (size_t i = 0; i < 100; ++i) {
    // All deserialised pointers are to the same instance
    assert(deserialisedInstances[0].get() == deserialisedInstances[i].get()); // true, they're pointers to the same address
    
    // BUT they don't point to the same instance as originalInstance
    assert(deserialisedInstances[i] == originalInstance.get()); // FALSE! During deserialisation an instance of `T` has been created on the heap!
}
````

The following pointers will not be to the same shared instance, despite being deserialised from the same source, because they are created over multiple calls to `esd::Deserialise`.
````C++
std::shared_ptr<T> originalInstance = std::make_shared<T>(args...);
nlohmann::json serialised = esd::Serialise(originalInstance);

// This DOES NOT point to the same instance as originalSharedInstance
std::shared_ptr<T> deserialisedInstance = esd::DeserialiseWithoutChecks<std::shared_ptr<T>>(serialised);

// This DOES NOT to the same instance as deserialisedSharedInstance
std::shared_ptr<T> anotherDeserialisedInstance = esd::DeserialiseWithoutChecks<std::shared_ptr<T>>(serialised);
````


This can be overcome if necessary by using an `esd::ContextStateLifetime`. As long as any `esd::ContextStateLifetime` instance exists anywhere, the internal caches will be preserved. This means that if multiple are created, the caches to be only cleared when the final one is destroyed.

````C++
std::shared_ptr<T> originalInstance = std::make_shared<T>(args...);
nlohmann::json serialised = esd::Serialise(originalInstance);

{
    esd::ContextStateLifetime scopedLifetime;

    // These pointers now point to the same shared instance
    std::shared_ptr<T> deserialisedInstance = esd::DeserialiseWithoutChecks<std::shared_ptr<T>>(serialised);
    std::shared_ptr<T> anotherDeserialisedInstance = esd::DeserialiseWithoutChecks<std::shared_ptr<T>>(serialised);
}

// `scopedLifetime` has gone out of scope, so now this pointer is to a new instance
std::shared_ptr<T> lastDeserialisedInstance = esd::DeserialiseWithoutChecks<std::shared_ptr<T>>(serialised);

````

## Examples
-----------

### Using the Library
---------------------

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
std::optional<GameState> LoadGame()
{
    std::ifstream file("save-game.txt");
    nlohmann::json serialisedGameState;
    file >> serialisedGameState;
    return esd::Deserialise<GameState>(serialisedGameState);
}
````
The other option is to do the checks ourselves
````C++
void LoadGame()
{
    std::ifstream file("save-game.txt");
    nlohmann::json serialisedGameState;
    file >> serialisedGameState;
    if (esd::Validate<GameState>(serialisedGameState)) {
        LoadGameState(esd::DeserialiseWithoutChecks<GameState>(serialisedGameState));
    } else {
        std::cout << "Failed to load save from save-game.txt" << std::endl;
    }
}
````

### Supporting A Trivial Type
-----------------------------

This is intended to be the most common use case. The following example demonstrates how you would use the esd::ClassHelper to support a struct, and optionally assign a label and/or additional validation to each struct member variable.
````C++
struct TrivialType {
    int a_;
    std::vector<int> b_;
    std::string c_;
};

template <>
struct esd::Serialiser<TrivialType> : public esd::ClassHelper<TrivialType, int, std::vector<int>, std::string> {
    static void Configure()
    {
        // Here we pass in static member object pointers that can be applied to an instance of the type later
        SetConstruction(CreateParameter(&TrivialType::a_), 
                        CreateParameter(&TrivialType::b_, "B"),
                        CreateParameter(&TrivialType::c_, std::nullopt, [](const std::string&) -> bool { /* custom validation */ }));
    }
};
````

[Back to Index](#Table-of-Contents)

### Supporting A Complex Type
-----------------------------

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

template <>
class esd::Serialiser<ContrivedType> : public esd::ClassHelper<ContrivedType, int> {
public:
    static void Configure()
    {
        // b_ is private, so instead we use a static member function pointer to its getter
        SetConstruction(CreateParameter(&ContrivedType::GetB));

        // First we specify what the initialisation call is, then we specify the parameters the same way we do for the constructor
        AddInitialisationCall(&ContrivedType::Initialise, CreateParameter(&ContrivedType::a_), CreateParameter(&ContrivedType::GetC));

        // Custom Getter or Setters can be provided for any variable or parameter, not only member pointers are supported
        auto customGetD = [](const ContrivedType& toSerialise) -> TrivialType
                          {
                              return { toSerialise.GetD_A(), toSerialise.GetD_B(), toSerialise.GetD_C() };
                          };
        // Register a variable that isn't covered in construction or via an initialiser (note that CreateParameter calls aren't necessary or supported here)
        RegisterVariable(std::move(customGetD), &ContrivedType::SetD);

        /*
         * The following two functions allow complete flexibility in how you serialise and 
         * deserialise your type, they should rarely be necesary if ever. Perhaps consider
         * if serialising a type that requires these is actually the correct strategy!
         */

        // Perhaps add something custom to the json, or do global housekeeping
        DefinePostSerialiseAction([](const ContrivedType& input, nlohmann::json& output){ /* ... */ });
        // Perhaps parse something custom from the json, or further validate the output, or do global housekeeping
        DefinePostDeserialiseAction([](const nlohmann::json& input, ContrivedType& output){ /* ... */ });
    }
};
````

[Back to Index](#Table-of-Contents)

## Supporting Templated Types
-------------------------------

This is identical to supporting a trivial or complex type, except function-name lookup can be problematic. To combat this you can use:
````C++
template <typename T>
struct TemplatedType {
    T value_;
    TemplatedType(T&& value);
};

template <typename T>
struct esd::Serialiser<TemplatedType<T>> : public esd::ClassHelper<TemplatedType, T> {
    static void Configure()
    {
        // `using` allows us to compress the typename for cases where it is very verbose
        using This = Serialiser<TemplatedType<T>>;
        
        // Apart from the prefix, `ClassHelper` functions can now be called normally
        This::SetConstruction(This::CreateParameter(&TemplatedType::value_));
    }
};
````

[Back to Index](#Table-of-Contents)

## Supporting Polymorphic Types
-------------------------------

And finally some examples showing how to support polymorphism. The class and `esd::Serialiser` specialisation implementation details have been ommited for brevity.

The polymorphic types to support:

````C++
class ParentType;
````
````C++
class ChildTypeA : public ParentType;
````
````C++
class ChildTypeB : public ParentType;
````
````C++
class GrandChildType : public ChildTypeA;
````

Each type needs to have a `Serialiser` specialisation. This step is identical to supporting non-polymorphic types.

If a specialisation already exists (e.g. your type has nlohmann::json's own `to_json` and `from_json` support, or satisfies `std::ranges::range<T>`) then you don't need to define your own specialisation.

````C++
template <>
class esd::Serialiser<ParentType> : public esd::ClassHelper<ParentType, int>;
````
````C++
template <>
class esd::Serialiser<ChildTypeA> : public esd::ClassHelper<ChildTypeA, bool>;
````
````C++
template <>
class esd::Serialiser<ChildTypeB>; // Extending ClassHelper is completely optional (though advised!)
````
````C++
template <>
class esd::Serialiser<GrandChildType> : public esd::ClassHelper<GrandChildType, int>;
````

To support the polymorphic aspect we can just add the following definition. 
Note that ParentType is listed twice in `PolymorphicSet<...>`. The first type in the list must match the type in `PolymorphismHelper<T>` and the following types are all of the types a ParentType* could be pointing to.

````C++
class PolymorphismHelper<ParentType> : public PolymorphicSet<ParentType, ParentType, ChildTypeA, ChildTypeB, GrandChildType>{}
````

Now any `shared_ptr<ParentType>` or `unique_ptr<ParentType>` will correctly serialise to any of the derived types (in this case `ParentType`, `ChildTypeA`, `ChildTypeB`, or `GrandChildType`).

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

##### Deserialise
-----------------

 If `esd::Validate<T>(serialised)` returns true, the returned optional will contain a valid instance of type T, else it will be equal to `std::nullopt`.
````C++
template <typename T> requires TypeSupportedByEasySerDes<T>
std::optional<T> Deserialise(const nlohmann::json& serialised);
````

[Back to Index](#Table-of-Contents)

##### DeserialiseWithoutChecks
------------------------------

`Validate<T>(serialised)` is not called, and should be first called by the user. If the json cannot be succesfully converted into an instance of type T the program will halt.
This may be useful for cases where type `T` cannot copied or moved out of a `std::optional<T>`.
````C++
template <typename T> requires TypeSupportedByEasySerDes<T>
T DeserialiseWithoutChecks(const nlohmann::json& serialised);
````

[Back to Index](#Table-of-Contents)

### esd::Serialiser
-------------------

For any of your types to be supported by this library, you must create a specialisation of the following templated type.
Every esd::Serialiser must implement a static Validate, Serialise and Deserialise function.
It is **NOT RECOMMENDED** that you support your types directly like this, see `esd::ClassHelper`.
````C++
template <>
class esd::Serialiser<T> {
public:
    static bool Validate(const nlohmann::json& serialised);
    static nlohmann::json Serialise(const T& value);
    static T Deserialise(const nlohmann::json& serialised);
};
````

[Back to Index](#Table-of-Contents)

### esd::ClassHelper
--------------------

To make use of the `ClassHelper` you publically extend it when you are implementing an esd::Serialiser.
It already defines the Validate, Serialise and Deserialise functions for you, so all that remains is to define a static Configure function as below.
````C++
template <>
class esd::Serialiser<T> : public esd::ClassHelper<T, ConstructionParameterTypes...> {
public:
    static void Configure();
};
````
#### esd::ClassHelper::SetConstruction
--------------------------------------

This **MUST** be called if your type is not default constructable, or if your type is default constructable and has an alternative constructor that you would like to use.
**Repeated calls to this function simply overwrite previous calls.**
`SetConstruction` takes a series of `Parameter`s which are created by the helper.
The actual type of `Parameter` is private in this context, so a Parameter has to be created in place within the call to `SetConstruction`.

````C++
static void Configure()
{
    SetConstruction(CreateParameter(...), ...);
}
````
`CreateParameter` is a template, and while the type will be **automatically deduced**, it is important that the deduced types match the target types construction signature, ignoring constness, references and r values e.t.c.
If the constructor takes int and const string reference
````C++
T(int a, const std::string& b);
````
The the parameters must be equivalent to the follwoing
````C++
SetConstruction(CreateParameter<int>(...), CreateParameter<std::string>(...));
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
SetConstruction(&ValidateFunc, CreateParameter<int>(...), CreateParameter<std::string>(...));
// OR use a lambda
SetConstruction([](int i, const std::string& s){ return i < s.size(); }, CreateParameter<int>(...), CreateParameter<std::string>(...));
````

[Back to Index](#Table-of-Contents)

#### esd::ClassHelper::AddInitialisationCall
--------------------------------------------

Some types may not complete their initialisation within a constructor, but instead require a subsequent call to some initialise function.
As many initialise calls can be registered as necessary.
`AddInitialisationCall` takes a member function pointer, to a public member function of the target type, and a series of `Parameter`s which are created by the helper.
The actual type of `Parameter` is private in this context, so a Parameter has to be created in place within the call to `AddInitialisationCall`.

````C++
class T {
public:
    Initialise(bool b, const std::string& s);
};
````
````C++
static void Configure()
{
    AddInitialisationCall(&T::Initialise, CreateParameter(...), CreateParameter(...));
}
````
`CreateParameter` is a template, and while the type will be **automatically deduced**, it is important that the deduced types match the provided initialise function's signature, ignoring constness, references and r values e.t.c.
In the above examplethe parameters must be equivalent to the follwoing
````C++
AddInitialisationCall(CreateParameter<bool>(...), CreateParameter<std::string>(...));
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
AddInitialisationCall(&ValidateFunc, CreateParameter<bool>(...), CreateParameter<std::string>(...));
// OR use a lambda
AddInitialisationCall([](bool b, const std::string& s){ return b == s.empty(); }, CreateParameter<bool>(...), CreateParameter<std::string>(...));
````

[Back to Index](#Table-of-Contents)

#### esd::ClassHelper::CreateParameter
--------------------------------------

This function is used with both `SetConstruction` and `AddInitialisationCall`, as both require `0` or more `CreateParameter` return types as input.
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

#### esd::ClassHelper::RegisterVariable
---------------------------------------

Some types may have member variables that are not set via a constructor or initialise function, and therefore need to be dealt with seperately.
There are a few overloads, which are similar to `CreateParameter`, except that we also need to specify a `setter` as well as the `getter`.

In the case where the variable is a public member of the type, `&T::memberVariableName_` can be used for both reading and writing the value.

The `getterAndSetter` parameter of the first overload can be:
|                Name                 |           Example         |
| ----------------------------------- | --------------------------|
| A public object member pointer      | `&T::memberVariableName_` |

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

#### esd::ClassHelper::DefinePostSerialiseAction
------------------------------------------------

This function is included alongside `DefinePostDeserialiseAction` to allow complete flexibility. It is not intended that these functions be used often.

This function provides complete access to the generated output just before it is serialised, allowing you to add any additional data you wish to save, and you can then read that data in an equal but opposite implementation passed to the `DefinePostDeserialiseAction` function. Again, this is not recommended.

You can also ignore the input parameters and simply use the function to perform some global book-keeping e.c.t.

````C++
void DefinePostSerialiseAction(std::function<void (const T&, nlohmann::json&)>&& action);
````

[Back to Index](#Table-of-Contents)

#### esd::ClassHelper::DefinePostDeserialiseAction
--------------------------------------------------

This function is included alongside `DefinePostSerialiseAction` to allow complete flexibility. It is not intended that these functions be used often.

This function provides complete access to an instance of `T` just after it is has been deserialised, allowing you to modify it in any way you desire, and you have access to parsed data allowing you to read any additional items you stored in your implementation passed to `DefinePostSerialiseAction`. Again, this is not recommended.

You can also ignore the input parameters and simply use the function to perform some global book-keeping e.c.t.

````C++
void DefinePostDeserialiseAction(std::function<void (const nlohmann::json&, T&)>&& action);
````

[Back to Index](#Table-of-Contents)

#### esd::ClassHelper::DeserialiseInPlace
-----------------------------------------

These additional deserialise functions are not utilised by `esd::Deserialise` or `esd::DeserialiseWithoutChecks`, but can be used manually by calling `Serialiser<T>::DeserialiseInPlace(...)` where `T` has a corresponding `Serialiser` specialisation that extends `ClassHelper<T, ConstructionArgs...>`.

In all cases, it is important that the instance of `T` created by the factory is accessible to `ClassHelper`, even if the factory doesn't return an instance directly.

The **first overload** is for cases where the `factory` returns an instance directly, or the result can be de-referenced, e.g. an iterator, or smart pointer.

````C++
auto DeserialiseInPlace(Invocable factory, const nlohmann::json& toDeserialise)
````

The **second overload** is for when the `factory` doesn't return an instance, or something that can be dereferenced into an instance. Instead the user must also provide a `accessResultFromFactoryOutput` parameter, which is a function that takes the output of the `factory` and returns the instance of `T` that was just created.

Note that `factory` may not return anything, or something that has no way to return an instance of `T`, in this case you'll need to use lambda capture.

````C++
auto DeserialiseInPlace(Invocable factory, const std::function<T* (FactoryReturnType&)>& accessResultFromFactoryOutput, const nlohmann::json& toDeserialise)
````

Now in cases where we know the type in question, and the construction args we could call the following to create a `shared_ptr<T>`
````C++
struct T { int i; double d; };
nlohmann::json serialised = ...;
Serialiser<T>::DeserialiseInPlace(std::make_shared<T, int, double>, serialised);
````

There is however a small problem, we cannot pass `std::make_shared` as a templateparameter, or an argument... `&std::make_shared<T>` could be passed because it is a fully formed function pointer, but note that in this case we are limiting ourselves to default constructable types. `std::make_shared<T>(32)` is actually deduced as `std::make_shared<T, int>(32)` by the compiler! Instead we need to let the compiler deduce the types for us using a templated lambda:
````C++
template <typename T>
nlohmann::json serialised = ...;
Serialiser<T>::DeserialiseInPlace([](auto... args){ return factory<T>(args...); }, serialised);
````

An example of this can be seen in the `std::shared_ptr` and `std::unique_ptr` specialisations, where `std::make_shared` and `std::make_unique` are used respectively.

The following code has been cut down for this demonstration.
````C++
template <typename T>
class esd::Serialiser<std::shared_ptr<T>> {
public:
    static std::shared_ptr<T> Deserialise(const nlohmann::json& serialised)
    {
        if constexpr (esd::HasClassHelperSpecialisation<T>) {
            return esd::Serialiser<T>::DeserialiseInPlace([](auto... args){ return std::make_shared<T>(args...); }, serialised.at(wrappedTypeKey));
        }
    }
````

[Back to Index](#Table-of-Contents)

### esd::PolymorphismHelper
---------------------------------------

To allow the library to treat polymorphic types correctly, you must define a `PolymorphismHelper` for each polymorphic type.

**Polymorphism is only supported through smart pointers.**

Each `esd::PolymorphismHelper<T>` extends `esd::PolymorphicSet<BaseType, DerivedTypes>`, where `T` must always be the same type as `BaseType` and `DerivedTypes` must all be derived from `BaseType`.

````C++
// Add polymorphic support for BaseType smart pointers
class esd::PolymorphismHelper<BaseType> : public esd::PolymorphicSet<BaseType, BaseType, ChildType, AnotherChildType, GrandChild, GreatGrandChildType>{};

struct Foo {
    // Polymorphic, these can point to instances of any of the derived types
    std::vector<std::shared_ptr<BaseType>> polymorphic;

    // NOT POLYMORPHIC! when saved and loaded these will all be sliced to instances of `GrandChild`!
    // FIX: `class esd::PolymorphismHelper<GrandChild> : public esd::PolymorphicSet<GrandChild, GrandChild, GreatGrandChildType>{};`
    std::vector<std::shared_ptr<GrandChild>> notPolymorphic;
};
````

It is worth noting that `T` counts as being derived from `T`, so it must be repeated in the `DerivedTypes` list if you intend to support instances of the base type too.

````C++
class PolymorphismHelper<PureVirtualBaseType> : public PolymorphicSet<PureVirtualBaseType, DerivedTypes...>{};
class PolymorphismHelper<ConstructableBaseType> : public PolymorphicSet<ConstructableBaseType, ConstructableBaseType, DerivedTypes...>{};
````


[Back to Index](#Table-of-Contents)

#### esd::PolymorphismHelper::ContainsPolymorphicType
-----------------------------------------------------

Takes a reference to a `const nlohmann::json& serialised`reference,
   - If it returns `true` if you need to call `PolymorphismHelper<T>::ValidatePolymorphic(serialised)` or `PolymorphismHelper<T>::DeserialisePolymorphic(serialised)`.
   - If it returns `false` if you need to call `esd::Validate<T>(serialised)` or `esd::Deserialise<T>(serialised)` (or `Serialiser<T>::`[DeserialiseInPlace](#esdClassHelperDeserialiseInPlace)`(...)`). 

[Back to Index](#Table-of-Contents)

#### esd::PolymorphismHelper::ValidatePolymorphic
-------------------------------------------------

Takes a `const nlohmann::json& serialised`reference and forwards the Validate call onto the correct child type's validator.

[Back to Index](#Table-of-Contents)

#### esd::PolymorphismHelper::SerialisePolymorphic
--------------------------------------------------

Takes a `BaseType& instance` reference and forwards the serialise call onto the correct child type's serialiser.

[Back to Index](#Table-of-Contents)

#### esd::PolymorphismHelper::DeserialisePolymorphic
----------------------------------------------------

Takes a `const nlohmann::json& serialised`reference and forwards the Deserialise call call onto `Serialiser<std::unique_ptr<CorrectDerivedType>>::Deserialise(serialised)`.

Because the `Serialiser<unique_ptr<T>>` calls `PolymorphismHelper<T>::DeserialisePolymorphic(serialised)` too, we need to avoid infinetely recursive calls, to achieve this, `DeserialisePolymorphic` **always makes a copy of serialised**, strips the marker that indicates it is polymorphic, and forwards the copy onto the final `Deserialise` call. 

[Back to Index](#Table-of-Contents)

## TODO
-------

 [x] Support std library types
 [x] Add tests
 [x] Create `ClassHelper` type to simplify support of user types
 [x] Support Polymorphism
 [ ] Inline Documentation of why I've created various types and functions, with intended purpose
 [x] Refactor project structure, `esd` directory for includes, remove the `EasySerDes` filename prefix
 [x] Refactor to remove Json prefixes from type and header names
 [x] Create a CurrentContext type and use it to store all caches
 [ ] Extend CurrentContext to include error reporting
 [x] Add tests for polymorphism with a pure virtual base class
 [ ] Push to Github
 [ ] Add some github extensions (test coverage, auto-running tests, linter e.t.c.)
 [ ] Zero warnings
 [ ] 100% test coverage
 [ ] Add to website and link to website in README
 [ ] In the "Adding It To Your Own Project" section, link to one of my projects that uses this library as a complete example
 [ ] MAYBE POD helper (that would implement the `Configure` function automatically)
 [ ] MAYBE Enum helper that allows the user to set a max and min value, or set allowed values, or set valid flags etc (and would implement the `Configure` function automatically)
 [ ] MAYBE Flags helper, like enum helper but also allows values which are enum values or'd together (and would implement the `Configure` function automatically)
 [ ] MAYBE use CurrentContext to abstract the underlying storage type
 [ ] MAYBE if underlying storage type is abstracted, use CurrentContext as ContextBase, and create "json" directory with JsonContext, and make CMAKE variable to exclude it unless requested
 [ ] MAYBE if JsonContext created, implement a basic std library class that extends ContextBase

[Back to Index](#Table-of-Contents)
