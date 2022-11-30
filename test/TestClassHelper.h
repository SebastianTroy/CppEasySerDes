#ifndef TESTCLASSHELPER_H
#define TESTCLASSHELPER_H

#include "esd/ClassHelper.h"

class TrivialTestType {
public:
    int a_;
    std::vector<int> b_;
    std::string c_;

    // Not necessary for EasySerDes but useful for testing
    bool operator==(const TrivialTestType& other) const = default;
};

class NonTrivialTestType {
public:
    bool b_;

    NonTrivialTestType(int a)
        : a_(a)
    {
    }

    const int& GetA() const
    {
        return a_;
    }

    // Not necessary for EasySerDes but useful for testing
    bool operator==(const NonTrivialTestType& other) const = default;

private:
    const int a_;
};

class InitialisedTestType {
public:
    void Initialise(int a, bool b, char c)
    {
        a_ = a;
        b_ = b;
        c_ = c;
    }

    const int& GetA() const
    {
        return a_;
    }

    const bool& GetB() const
    {
        return b_;
    }

    const char& GetC() const
    {
        return c_;
    }

    // Not necessary for EasySerDes but useful for testing
    bool operator==(const InitialisedTestType& other) const = default;

private:
    int a_;
    bool b_;
    char c_;
};

class NestedTestType {
public:
    TrivialTestType b_;

    NestedTestType(const NestedTestType& other) = default;
    NestedTestType(NestedTestType&& other) = default;
    NestedTestType(NonTrivialTestType a)
        : a_(a)
    {
    }

    const NonTrivialTestType& GetA() const
    {
        return a_;
    }

    // Not necessary for EasySerDes but useful for testing
    bool operator==(const NestedTestType& other) const = default;

private:
    NonTrivialTestType a_;
};

/*
 * Some operators to make testing easier
 */

template <typename T>
std::ostream& operator<<(std::ostream& ostr, const std::vector<T>& vector)
{
    ostr << "{";
    for (const auto& value : vector) {
        ostr << " " << value << ",";
    }
    return ostr << " }";
}

std::ostream& operator<<(std::ostream& ostr, const TrivialTestType& value)
{
    return ostr << "TrivialTestType{" << value.a_ << ", " << value.b_ << ", " << value.c_ << "}";
}

std::ostream& operator<<(std::ostream& ostr, const NonTrivialTestType& value)
{
    return ostr << "NonTrivialTestType{" << value.GetA() << ", " << value.b_ << "}";
}

std::ostream& operator<<(std::ostream& ostr, const InitialisedTestType& value)
{
    return ostr << "InitialisedTestType{" << value.GetA() << ", " << value.GetB() << ", " << value.GetC() << ", " << "}";
}

std::ostream& operator<<(std::ostream& ostr, const NestedTestType& value)
{
    return ostr << "NestedTestType{" << value.GetA() << ", " << value.b_ << "}";
}

/*
 * EasySerDes integration
 */

namespace esd {

template <>
class Serialiser<TrivialTestType> : public ClassHelper<TrivialTestType, int, std::vector<int>, std::string> {
public:
    static void Configure()
    {
        SetConstruction(CreateParameter(&TrivialTestType::a_),
                        CreateParameter(&TrivialTestType::b_),
                        CreateParameter(&TrivialTestType::c_));
    }
};

template <>
class Serialiser<NonTrivialTestType> : public ClassHelper<NonTrivialTestType, int> {
public:
    static void Configure()
    {
        SetConstruction(CreateParameter(&NonTrivialTestType::GetA));
        RegisterVariable(&NonTrivialTestType::b_);
    }
};

template <>
class Serialiser<InitialisedTestType> : public ClassHelper<InitialisedTestType> {
public:
    static void Configure()
    {
        AddInitialisationCall(&InitialisedTestType::Initialise,
                              CreateParameter(&InitialisedTestType::GetA),
                              CreateParameter(&InitialisedTestType::GetB),
                              CreateParameter(&InitialisedTestType::GetC));
    }
};

template <>
class Serialiser<NestedTestType> : public ClassHelper<NestedTestType, NonTrivialTestType> {
public:
    static void Configure()
    {
        SetConstruction(CreateParameter(&NestedTestType::GetA));
        RegisterVariable(&NestedTestType::b_);
    }
};

/*
 * Test that certain member types prevent compilation, and that ClassHelper
 * disallows pointer and reference ConstructionArgs (because they'll only ever
 * point to tempory values!)
 */
class TypeWithNormalString {
public:
    std::string c_;
};

class TypeWithReference {
public:
    std::string& c_;
};

class TypeWithConstReference {
public:
    const std::string& c_;
};

class TypeWithPointer {
public:
    std::string* c_;
};

template <typename T, typename... Args>
concept TypeIsSupported = requires (nlohmann::json j) {
    { esd::ClassHelper<T, Args...>::SetConstruction(esd::ClassHelper<T, Args...>::CreateParameter(&T::c_, "c")) };
    { esd::ClassHelper<T, Args...>::Deserialise(j) } -> std::same_as<T>;
};

static_assert(TypeIsSupported<TypeWithNormalString, std::string>);
static_assert(!TypeIsSupported<TypeWithNormalString, std::string&>);
static_assert(!TypeIsSupported<TypeWithNormalString, std::string*>);

static_assert(!TypeIsSupported<TypeWithReference, std::string>);
static_assert(!TypeIsSupported<TypeWithReference, std::string&>);
static_assert(!TypeIsSupported<TypeWithReference, std::string*>);

static_assert(!TypeIsSupported<TypeWithConstReference, std::string>);
static_assert(!TypeIsSupported<TypeWithConstReference, std::string&>);
static_assert(!TypeIsSupported<TypeWithConstReference, std::string*>);

static_assert(!TypeIsSupported<TypeWithPointer, std::string>);
static_assert(!TypeIsSupported<TypeWithPointer, std::string&>);
static_assert(!TypeIsSupported<TypeWithPointer, std::string*>);


} // end namespace esd

#endif // TESTCLASSHELPER_H
