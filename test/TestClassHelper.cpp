#include "TestClassHelper.h"

#include "EasySerDes.h"

#include <catch2/catch.hpp>

using namespace nlohmann;
using namespace esd;

TEST_CASE("InternalHelper", "[json]")
{
    struct MinMax {
        int min, max;

        void SetMinMax(int min, int max)
        {
            this->min = min;
            this->max = max;
        }
    };

    auto validator = [](const int& min, const int& max) -> bool
    {
        return min <= max;
    };

    SECTION("RegisterConstruction")
    {
        InternalHelper<MinMax, int, int> h;
        h.RegisterConstruction(h.CreateParameter(&MinMax::min, "min"), h.CreateParameter(&MinMax::max, "max"), validator);

        REQUIRE(h.Validate({ {"min", -10}, {"max", 42} }));
        REQUIRE_FALSE(h.Validate({ {"min", 10}, {"max", 0} }));
        REQUIRE_FALSE(h.Validate({ {"min", "String"}, {"max", 0} }));
        REQUIRE_FALSE(h.Validate({ {"min", 10.5}, {"max", 0} }));
        REQUIRE_FALSE(h.Validate({ {"min", 10.0}, {"max", 0} }));

        MinMax original{-5432, 1346};
        auto serialised = h.Serialise(original);
        REQUIRE(h.Validate(serialised));
        MinMax deserialised = h.Deserialise(serialised);
        REQUIRE(original.min == deserialised.min);
        REQUIRE(original.max == deserialised.max);
    }

    SECTION("RegisterConstruction with factory")
    {
        auto factory = [](int min, int max) -> MinMax
        {
            return { min, max };
        };
        InternalHelper<MinMax, int, int> h;
        h.RegisterConstruction(h.CreateParameter(&MinMax::min, "min"), h.CreateParameter(&MinMax::max, "max"), factory, validator);

        REQUIRE(h.Validate({ {"min", -10}, {"max", 42} }));
        REQUIRE_FALSE(h.Validate({ {"min", 10}, {"max", 0} }));
        REQUIRE_FALSE(h.Validate({ {"min", "String"}, {"max", 0} }));
        REQUIRE_FALSE(h.Validate({ {"min", 10.5}, {"max", 0} }));
        REQUIRE_FALSE(h.Validate({ {"min", 10.0}, {"max", 0} }));

        MinMax original{-5432, 1346};
        auto serialised = h.Serialise(original);
        REQUIRE(h.Validate(serialised));
        MinMax deserialised = h.Deserialise(serialised);
        REQUIRE(original.min == deserialised.min);
        REQUIRE(original.max == deserialised.max);
    }

    SECTION("RegisterInitialisation")
    {
        InternalHelper<MinMax, int, int> h;
        h.RegisterInitialisation(validator, &MinMax::SetMinMax, h.CreateParameter(&MinMax::min, "min"), h.CreateParameter(&MinMax::max, "max"));

        REQUIRE(h.Validate({ {"min", -10}, {"max", 42} }));
        REQUIRE_FALSE(h.Validate({ {"min", 10}, {"max", 0} }));
        REQUIRE_FALSE(h.Validate({ {"min", "String"}, {"max", 0} }));
        REQUIRE_FALSE(h.Validate({ {"min", 10.5}, {"max", 0} }));
        REQUIRE_FALSE(h.Validate({ {"min", 10.0}, {"max", 0} }));

        MinMax original{-5432, 1346};
        auto serialised = h.Serialise(original);
        REQUIRE(h.Validate(serialised));
        MinMax deserialised = h.Deserialise(serialised);
        REQUIRE(original.min == deserialised.min);
        REQUIRE(original.max == deserialised.max);
    }

    SECTION("RegisterVariable")
    {
        InternalHelper<MinMax, int, int> h;
        h.RegisterVariable(&MinMax::min, "min", [](const int& min) { return min < 0; });
        h.RegisterVariable(&MinMax::max, "max", [](const int& max) { return max > 0; });

        REQUIRE(h.Validate({ {"min", -10}, {"max", 42} }));
        REQUIRE_FALSE(h.Validate({ {"min", 10}, {"max", 0} }));
        REQUIRE_FALSE(h.Validate({ {"min", "String"}, {"max", 0} }));
        REQUIRE_FALSE(h.Validate({ {"min", 10.5}, {"max", 0} }));
        REQUIRE_FALSE(h.Validate({ {"min", 10.0}, {"max", 0} }));

        MinMax original{-5432, 1346};
        auto serialised = h.Serialise(original);
        REQUIRE(h.Validate(serialised));
        MinMax deserialised = h.Deserialise(serialised);
        REQUIRE(original.min == deserialised.min);
        REQUIRE(original.max == deserialised.max);
    }

    SECTION("DefinePostSerialisationAction")
    {
        int count = 0;

        auto action = [&](const MinMax&, json&)
        {
            count++;
        };

        InternalHelper<MinMax, int, int> h;
        h.DefinePostSerialiseAction(action);

        h.RegisterConstruction(h.CreateParameter(&MinMax::min, "min"), h.CreateParameter(&MinMax::max, "max"), std::move(validator));
        REQUIRE(count == 0);
        h = {};
        h.DefinePostSerialiseAction(action);
        h.RegisterInitialisation(std::move(validator), &MinMax::SetMinMax, h.CreateParameter(&MinMax::min, "min"), h.CreateParameter(&MinMax::max, "max"));
        REQUIRE(count == 0);
        h = {};
        h.DefinePostSerialiseAction(action);
        h.RegisterVariable(&MinMax::min, "min", [](const int& min) { return min < 0; });
        REQUIRE(count == 0);
        h.RegisterVariable(&MinMax::max, "max", [](const int& max) { return max > 0; });
        REQUIRE(count == 0);

        for (int i = 0; i < 10; ++i) {
            [[maybe_unused]] auto d = h.Deserialise({ {"min", -10}, {"max", 42} });
            REQUIRE(count == 0);
            [[maybe_unused]] auto v = h.Validate({ {"min", -10}, {"max", 42} });
            REQUIRE(count == 0);
        }

        for (int i = 0; i < 10; ++i) {
            REQUIRE(count == i);
            [[maybe_unused]] auto s = h.Serialise({-1, 1});
            REQUIRE(count == i + 1);
        }
    }

    SECTION("DefinePostDeserialisationAction")
    {
        int count = 0;

        auto action = [&](const json&, MinMax&)
        {
            count++;
        };

        InternalHelper<MinMax, int, int> h;
        h.DefinePostDeserialiseAction(action);

        h.RegisterConstruction(h.CreateParameter(&MinMax::min, "min"), h.CreateParameter(&MinMax::max, "max"), std::move(validator));
        REQUIRE(count == 0);
        h = {};
        h.DefinePostDeserialiseAction(action);
        h.RegisterInitialisation(std::move(validator), &MinMax::SetMinMax, h.CreateParameter(&MinMax::min, "min"), h.CreateParameter(&MinMax::max, "max"));
        REQUIRE(count == 0);
        h = {};
        h.DefinePostDeserialiseAction(action);
        h.RegisterVariable(&MinMax::min, "min", [](const int& min) { return min < 0; });
        REQUIRE(count == 0);
        h.RegisterVariable(&MinMax::max, "max", [](const int& max) { return max > 0; });
        REQUIRE(count == 0);

        for (int i = 0; i < 10; ++i) {
            [[maybe_unused]] auto s = h.Serialise({-1, 1});
            REQUIRE(count == 0);
            [[maybe_unused]] auto v = h.Validate({ {"min", -10}, {"max", 42} });
            REQUIRE(count == 0);
        }

        for (int i = 0; i < 10; ++i) {
            REQUIRE(count == i);
            [[maybe_unused]] auto d = h.Deserialise({ {"min", -10}, {"max", 42} });
            REQUIRE(count == i + 1);
        }
    }
}

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
