#ifndef TESTPOLYMORPHICCLASSHELPER_H
#define TESTPOLYMORPHICCLASSHELPER_H

#include "EasySerDesPolymorphicClassHelper.h"
#include "EasySerDesBuiltInTypeSupport.h"

#include <iostream>

class BaseTestType {
public:
    constexpr static int Value = 42;
    double d_;

    BaseTestType(double d)
        : d_(d)
    {
    }

    virtual ~BaseTestType(){}

    virtual int GetVal() const
    {
        return Value;
    }

    bool operator==(const BaseTestType& other) const = default;
};

class ChildTestTypeA : public BaseTestType {
public:
    constexpr static int Value = 79;

    ChildTestTypeA(double d)
        : BaseTestType(d)
    {
    }

    virtual ~ChildTestTypeA(){}

    virtual int GetVal() const override
    {
        return Value;
    }

    bool operator==(const ChildTestTypeA& other) const = default;
};

class ChildTestTypeB : public BaseTestType {
public:
    constexpr static int Value = 44;
    bool b_;

    ChildTestTypeB(double d, bool b)
        : BaseTestType(d)
        , b_(b)
    {
    }

    virtual ~ChildTestTypeB(){}

    virtual int GetVal() const override
    {
        return Value;
    }

    bool operator==(const ChildTestTypeB& other) const = default;
};

class GrandChildTestType : public ChildTestTypeB {
public:
    constexpr static int Value = 1000000;

    GrandChildTestType(double d, bool b)
        : ChildTestTypeB(d, b)
    {
    }

    virtual ~GrandChildTestType(){}

    virtual int GetVal() const override
    {
        return Value;
    }

    bool operator==(const GrandChildTestType& other) const = default;
};

/*
 * Some operators to make testing easier
 */

std::ostream& operator<<(std::ostream& ostr, const BaseTestType& value)
{
    return ostr << "BaseTestType{" << value.GetVal() << "}";
}

std::ostream& operator<<(std::ostream& ostr, const ChildTestTypeA& value)
{
    return ostr << "ChildTestTypeA{" << value.GetVal() << "}";
}

std::ostream& operator<<(std::ostream& ostr, const ChildTestTypeB& value)
{
    return ostr << "ChildTestTypeB{" << value.GetVal() << "}";
}

std::ostream& operator<<(std::ostream& ostr, const GrandChildTestType& value)
{
    return ostr << "GrandChildTestType{" << value.GetVal() << "}";
}

/*
 * EasySerDes integration
 */

// FIXME put the JsonSerialiser<> declaration outside of the esd namespace so users can override specific serialisers if tehy want to
template<>
class esd::JsonSerialiser<GrandChildTestType> : public esd::JsonPolymorphicClassSerialiser<GrandChildTestType, double, bool> {
public:
    static void SetupHelper(HelperType& h)
    {
        h.RegisterConstruction(h.CreateParameter(&GrandChildTestType::d_),
                               h.CreateParameter(&GrandChildTestType::b_));
    }
};

template<>
class esd::JsonSerialiser<ChildTestTypeB> : public esd::JsonPolymorphicClassSerialiser<ChildTestTypeB, double, bool> {
public:
    static void SetupHelper(HelperType& h)
    {
        h.RegisterConstruction(h.CreateParameter(&ChildTestTypeB::d_),
                               h.CreateParameter(&ChildTestTypeB::b_));
        RegisterChild<GrandChildTestType>();
    }
};

template<>
class esd::JsonSerialiser<ChildTestTypeA> : public esd::JsonPolymorphicClassSerialiser<ChildTestTypeA, double> {
public:
    static void SetupHelper(HelperType& h)
    {
        h.RegisterConstruction(h.CreateParameter(&ChildTestTypeA::d_));
    }
};

template<>
class esd::JsonSerialiser<BaseTestType> : public esd::JsonPolymorphicClassSerialiser<BaseTestType, double> {
public:
    static void SetupHelper(HelperType& h)
    {
        h.RegisterConstruction(h.CreateParameter(&BaseTestType::d_));

        RegisterChild<ChildTestTypeA>();
        RegisterChild<ChildTestTypeB>();

        // FIXME the following line should be prohibited by a constraint, but it compiles and causes errors
        // RegisterChild<GrandChildTestType>();
    }
};

#endif // TESTPOLYMORPHICCLASSHELPER_H
