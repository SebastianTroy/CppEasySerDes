#include "TestPolymorphicClassHelper.h"

#include "EasySerDesStdLibSupport.h"

#include <catch2/catch.hpp>

using namespace nlohmann;
using namespace esd;

TEST_CASE("Polymorphic types treated non-polymorphically", "[json]")
{
    SECTION("BaseTestType")
    {
        BaseTestType original { 44.79 };

        json serialised = esd::Serialise(original);
        REQUIRE(esd::Validate<BaseTestType>(serialised));

        BaseTestType deserialised = esd::DeserialiseWithoutChecks<BaseTestType>(serialised);
        json deserialisedReserialised = esd::Serialise(deserialised);

        // test this first so failures print out the before and after JSON, instead of a less helpful failed comparison
        REQUIRE(deserialisedReserialised == serialised);
        REQUIRE(deserialised == original);
    }

    SECTION("ChildTestTypeA")
    {
        ChildTestTypeA original { 77.32 };

        json serialised = esd::Serialise(original);
        REQUIRE(esd::Validate<ChildTestTypeA>(serialised));

        ChildTestTypeA deserialised = esd::DeserialiseWithoutChecks<ChildTestTypeA>(serialised);
        json deserialisedReserialised = esd::Serialise<ChildTestTypeA>(deserialised);

        // test this first so failures print out the before and after JSON, instead of a less helpful failed comparison
        REQUIRE(deserialisedReserialised == serialised);
        REQUIRE(deserialised == original);
    }

    SECTION("ChildTestTypeB")
    {
        ChildTestTypeB original { 79.32, true };

        json serialised = esd::Serialise(original);
        REQUIRE(esd::Validate<ChildTestTypeB>(serialised));

        ChildTestTypeB deserialised = esd::DeserialiseWithoutChecks<ChildTestTypeB>(serialised);
        json deserialisedReserialised = esd::Serialise<ChildTestTypeB>(deserialised);

        // test this first so failures print out the before and after JSON, instead of a less helpful failed comparison
        REQUIRE(deserialisedReserialised == serialised);
        REQUIRE(deserialised == original);
    }


    SECTION("GrandChildTestType")
    {
        GrandChildTestType original { 524.21345546, false };

        json serialised = esd::Serialise(original);
        REQUIRE(esd::Validate<GrandChildTestType>(serialised));

        GrandChildTestType deserialised = esd::DeserialiseWithoutChecks<GrandChildTestType>(serialised);
        json deserialisedReserialised = esd::Serialise<GrandChildTestType>(deserialised);

        // test this first so failures print out the before and after JSON, instead of a less helpful failed comparison
        REQUIRE(deserialisedReserialised == serialised);
        REQUIRE(deserialised == original);
    }

    SECTION("std::vector<GrandChildTestType>")
    {
        using TestType = std::vector<GrandChildTestType>;
        TestType original;

        for (int i = 0; i < 10; ++i) {
            double d = 12.123 * i;
            bool b = (i % 2 == 0);
            original.emplace_back(d, b);
        }

        json serialised = esd::Serialise(original);
        REQUIRE(esd::Validate<TestType>(serialised));

        TestType deserialised = esd::DeserialiseWithoutChecks<TestType>(serialised);
        json deserialisedReserialised = esd::Serialise(deserialised);

        // test this first so failures print out the before and after JSON, instead of a less helpful failed comparison
        REQUIRE(deserialisedReserialised == serialised);
        REQUIRE(deserialised == original);
    }
}

TEST_CASE("Polymorphic types treated polymorphically", "[json]")
{
    SECTION("std::unique_ptr")
    {
        using TestType = std::unique_ptr<BaseTestType>;
        TestType original = std::make_unique<GrandChildTestType>(4532.23465, true);

        auto originalValue = original->GetVal();

        json serialised = esd::Serialise(original);
        REQUIRE(esd::Validate<TestType>(serialised));
        REQUIRE(esd::JsonPolymorphicClassSerialiser<GrandChildTestType, double, bool>::Validate(serialised));

        TestType deserialised = esd::DeserialiseWithoutChecks<TestType>(serialised);
        json deserialisedReserialised = esd::Serialise(deserialised);

        auto deserialisedValue = deserialised->GetVal();
        REQUIRE(originalValue == deserialisedValue);

        // test this first so failures print out the before and after JSON, instead of a less helpful failed comparison
        REQUIRE(deserialisedReserialised.at("wrappedType") == serialised.at("wrappedType"));
        REQUIRE(*deserialised == *original);
    }

    SECTION("std::shared_ptr")
    {
        using TestType = std::shared_ptr<BaseTestType>;
        TestType original = std::make_unique<ChildTestTypeB>(4532.23465, true);

        auto originalValue = original->GetVal();

        json serialised = esd::Serialise(original);
        REQUIRE(esd::Validate<TestType>(serialised));
        REQUIRE(esd::JsonPolymorphicClassSerialiser<ChildTestTypeB, double, bool>::Validate(serialised));

        TestType deserialised = esd::DeserialiseWithoutChecks<TestType>(serialised);
        json deserialisedReserialised = esd::Serialise(deserialised);

        auto deserialisedValue = deserialised->GetVal();
        REQUIRE(originalValue == deserialisedValue);

        // test this first so failures print out the before and after JSON, instead of a less helpful failed comparison
        REQUIRE(deserialisedReserialised.at("wrappedType") == serialised.at("wrappedType"));
        REQUIRE(*deserialised == *original);
    }
}
