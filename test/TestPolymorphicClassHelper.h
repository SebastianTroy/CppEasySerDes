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

template<>
class esd::JsonSerialiser<GrandChildTestType> : public esd::JsonPolymorphicClassSerialiser<GrandChildTestType, double, bool> {
public:
    static void Configure()
    {
        SetConstruction(CreateParameter(&GrandChildTestType::d_),
                        CreateParameter(&GrandChildTestType::b_));
    }
};

template<>
class esd::JsonSerialiser<ChildTestTypeB> : public esd::JsonPolymorphicClassSerialiser<ChildTestTypeB, double, bool> {
public:
    static void Configure()
    {
        SetConstruction(CreateParameter(&ChildTestTypeB::d_),
                        CreateParameter(&ChildTestTypeB::b_));
        SetChildTypes<GrandChildTestType>();
    }
};

template<>
class esd::JsonSerialiser<ChildTestTypeA> : public esd::JsonPolymorphicClassSerialiser<ChildTestTypeA, double> {
public:
    static void Configure()
    {
        SetConstruction(CreateParameter(&ChildTestTypeA::d_));
    }
};

template<>
class esd::JsonSerialiser<BaseTestType> : public esd::JsonPolymorphicClassSerialiser<BaseTestType, double> {
public:
    static void Configure()
    {
        SetConstruction(CreateParameter(&BaseTestType::d_));

        SetChildTypes<ChildTestTypeA, ChildTestTypeB>();
    }
};

#endif // TESTPOLYMORPHICCLASSHELPER_H
