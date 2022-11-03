#ifndef TESTCLASSHELPER_H
#define TESTCLASSHELPER_H

#include "EasySerDesClassHelper.h"

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

template<>
class JsonSerialiser<TrivialTestType> : public JsonClassSerialiser<TrivialTestType, int, std::vector<int>, std::string> {
public:
    static void SetupHelper(HelperType& h)
    {
        h.RegisterConstruction(h.CreateParameter(&TrivialTestType::a_),
                               h.CreateParameter(&TrivialTestType::b_),
                               h.CreateParameter(&TrivialTestType::c_));
    }
};

template<>
class JsonSerialiser<NonTrivialTestType> : public JsonClassSerialiser<NonTrivialTestType, int> {
public:
    static void SetupHelper(HelperType& h)
    {
        h.RegisterConstruction(h.CreateParameter(&NonTrivialTestType::GetA));
        h.RegisterVariable(&NonTrivialTestType::b_);
    }
};

template<>
class JsonSerialiser<InitialisedTestType> : public JsonClassSerialiser<InitialisedTestType> {
public:
    static void SetupHelper(HelperType& h)
    {
        h.RegisterInitialisation(&InitialisedTestType::Initialise,
                                 h.CreateParameter(&InitialisedTestType::GetA),
                                 h.CreateParameter(&InitialisedTestType::GetB),
                                 h.CreateParameter(&InitialisedTestType::GetC));
    }
};

template<>
class JsonSerialiser<NestedTestType> : public JsonClassSerialiser<NestedTestType, NonTrivialTestType> {
public:
    static void SetupHelper(HelperType& h)
    {
        h.RegisterConstruction(h.CreateParameter(&NestedTestType::GetA));
        h.RegisterVariable(&NestedTestType::b_);
    }
};

} // end namespace esd

#endif // TESTCLASSHELPER_H
