#include "TestClassHelper.h"

#include "EasySerDes.h"

#include <catch2/catch.hpp>

using namespace nlohmann;
using namespace esd;

TEST_CASE("TrivialTestType", "[json]")
{
    TrivialTestType original { 79, { 42, 44, 79 }, "foobar" };

    json serialised = esd::Serialise(original);
    REQUIRE(esd::Validate<TrivialTestType>(serialised));

    TrivialTestType deserialised = esd::DeserialiseWithoutChecks<TrivialTestType>(serialised);
    json deserialisedReserialised = esd::Serialise(deserialised);

    // test this first so failures print out the before and after JSON, instead of a less helpful failed comparison
    REQUIRE(deserialisedReserialised == serialised);
    REQUIRE(deserialised == original);
}

TEST_CASE("TestNonTrivial", "[json]")
{
    NonTrivialTestType original { 77 };
    original.b_ = false;

    json serialised = esd::Serialise(original);
    REQUIRE(esd::Validate<NonTrivialTestType>(serialised));

    NonTrivialTestType deserialised = esd::DeserialiseWithoutChecks<NonTrivialTestType>(serialised);
    json deserialisedReserialised = esd::Serialise<NonTrivialTestType>(deserialised);

    // test this first so failures print out the before and after JSON, instead of a less helpful failed comparison
    REQUIRE(deserialisedReserialised == serialised);
    REQUIRE(deserialised == original);
}

TEST_CASE("std::vector<TrivialTestType>")
{
    std::vector<TrivialTestType> original;

    for (int i = 0; i < 10; ++i) {
        auto item = TrivialTestType{ i, std::vector<int>(i, 42), std::to_string(i) };
        item.a_ = i % 2 == 0;
        original.push_back(std::move(item));
    }

    json serialised = esd::Serialise(original);
    REQUIRE(esd::Validate<decltype(original)>(serialised));

    std::vector<TrivialTestType> deserialised = { esd::DeserialiseWithoutChecks<decltype(original)>(serialised) };
    json deserialisedReserialised = esd::Serialise(deserialised);

    // test this first so failures print out the before and after JSON, instead of a less helpful failed comparison
    REQUIRE(deserialisedReserialised == serialised);
    REQUIRE(deserialised == original);
}

TEST_CASE("NestedTestType", "[json]")
{
    NestedTestType original { 77 };
    original.b_ = { 42, {}, "Foo" };

    json serialised = esd::Serialise(original);
    REQUIRE(esd::Validate<NestedTestType>(serialised));

    NestedTestType deserialised = esd::DeserialiseWithoutChecks<NestedTestType>(serialised);
    json deserialisedReserialised = esd::Serialise<NestedTestType>(deserialised);

    // test this first so failures print out the before and after JSON, instead of a less helpful failed comparison
    REQUIRE(deserialisedReserialised == serialised);
    REQUIRE(deserialised == original);
}
