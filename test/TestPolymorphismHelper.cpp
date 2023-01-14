#include "TestPolymorphismHelper.h"

#include <EasySerDes.h>

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
    esd::Context c;

    SECTION("std::unique_ptr")
    {
        std::unique_ptr<BaseTestType> original = std::make_unique<GrandChildTestType>(4532.23465, true);

        auto originalValue = original->GetVal();

        json serialised = esd::Serialise(c, original);
        REQUIRE(esd::Validate<std::unique_ptr<BaseTestType>>(c, serialised));

        // use insider knowledge of how smart pointers are serialised to test manually
        REQUIRE_FALSE(esd::Validate<BaseTestType>(c, serialised));
        REQUIRE_FALSE(esd::Validate<GrandChildTestType>(c, serialised));
        REQUIRE(esd::PolymorphismHelper<BaseTestType>::ValidatePolymorphic(c, serialised));

        std::unique_ptr<BaseTestType> deserialised = esd::DeserialiseWithoutChecks<std::unique_ptr<BaseTestType>>(c, serialised);
        json deserialisedReserialised = esd::Serialise(c, deserialised);

        auto deserialisedValue = deserialised->GetVal();
        REQUIRE(originalValue == deserialisedValue);

        // test this first so failures print out the before and after JSON, instead of a less helpful failed comparison
        REQUIRE(deserialisedReserialised == serialised);
        REQUIRE(*deserialised == *original);
    }

    SECTION("std::shared_ptr<BaseTestType>")
    {
        std::shared_ptr<BaseTestType> original = std::make_unique<ChildTestTypeB>(4532.23465, true);

        auto originalValue = original->GetVal();

        json serialised = esd::Serialise(c, original);
        REQUIRE(esd::Validate<std::shared_ptr<BaseTestType>>(c, serialised));

        // use insider knowledge of how smart pointers are serialised to test manually
        auto objectDataKey = esd::Serialiser<std::shared_ptr<BaseTestType>>::wrappedTypeKey;
        REQUIRE_FALSE(esd::Validate<BaseTestType>(c, serialised.at(objectDataKey)));
        REQUIRE_FALSE(esd::Validate<ChildTestTypeB>(c, serialised.at(objectDataKey)));
        REQUIRE(esd::PolymorphismHelper<BaseTestType>::ValidatePolymorphic(c, serialised.at(objectDataKey)));

        std::shared_ptr<BaseTestType> deserialised = esd::DeserialiseWithoutChecks<std::shared_ptr<BaseTestType>>(c, serialised);
        json deserialisedReserialised = esd::Serialise(c, deserialised);

        auto deserialisedValue = deserialised->GetVal();
        REQUIRE(originalValue == deserialisedValue);

        // test this first so failures print out the before and after JSON, instead of a less helpful failed comparison
        REQUIRE(deserialisedReserialised.at(objectDataKey) == serialised.at(objectDataKey));
        REQUIRE(*deserialised == *original);
    }

    SECTION("std::vector<std::shared_ptr<BaseTestType>>")
    {
        std::vector<std::shared_ptr<BaseTestType>> original{std::make_shared<GrandChildTestType>(4532.23465, true),
                                                            std::make_shared<BaseTestType>(543.2345),
                                                            std::make_shared<ChildTestTypeA>(9654.321465),
                                                            std::make_shared<ChildTestTypeB>(64532.898323, false)
                                                           };

        json serialised = esd::Serialise(original);

        REQUIRE(esd::Validate<decltype(original)>(serialised));

        auto deserialised = esd::DeserialiseWithoutChecks<decltype(original)>(serialised);
        json deserialisedReserialised = esd::Serialise(deserialised);

        REQUIRE(original.size() == deserialised.size());
        for (std::size_t i = 0; i < original.size(); ++i) {
            auto& originalPtr = original[i];
            auto& deserialisedPtr = deserialised[i];

            auto objectDataKey = esd::Serialiser<std::shared_ptr<BaseTestType>>::wrappedTypeKey;
            // test this first so failures print out the "before and after" JSON, instead of a less helpful failed comparison
            REQUIRE(deserialisedReserialised.at(i).at(objectDataKey) == serialised.at(i).at(objectDataKey));
            REQUIRE(*deserialisedPtr == *originalPtr);
        }
    }

    SECTION("std::shared_ptr<PureVirtualInterface>")
    {
        std::shared_ptr<PureVirtualInterface> original = std::make_unique<ChildTestTypeB>(4532.23465, true);

        auto originalValue = original->GetVal();

        json serialised = esd::Serialise(c, original);
        REQUIRE(esd::Validate<std::shared_ptr<PureVirtualInterface>>(c, serialised));

        // use insider knowledge of how smart pointers are serialised to test manually
        auto objectDataKey = esd::Serialiser<std::shared_ptr<BaseTestType>>::wrappedTypeKey;
        // REQUIRE_FALSE(esd::Validate<PureVirtualInterface>(c, serialised.at(objectDataKey))); Doesn't exist because type is pure virtual
        REQUIRE_FALSE(esd::Validate<ChildTestTypeB>(c, serialised.at(objectDataKey)));
        REQUIRE(esd::PolymorphismHelper<PureVirtualInterface>::ValidatePolymorphic(c, serialised.at(objectDataKey)));

        std::shared_ptr<PureVirtualInterface> deserialised = esd::DeserialiseWithoutChecks<std::shared_ptr<PureVirtualInterface>>(c, serialised);
        json deserialisedReserialised = esd::Serialise(c, deserialised);

        auto deserialisedValue = deserialised->GetVal();
        REQUIRE(originalValue == deserialisedValue);

        // test this first so failures print out the before and after JSON, instead of a less helpful failed comparison
        REQUIRE(deserialisedReserialised.at(objectDataKey) == serialised.at(objectDataKey));
        REQUIRE(*deserialised == *original);
    }

    SECTION("std::vector<std::shared_ptr<PureVirtualInterface>>")
    {
        std::vector<std::shared_ptr<PureVirtualInterface>> original{std::make_shared<GrandChildTestType>(4532.23465, true),
                                                                    std::make_shared<BaseTestType>(543.2345),
                                                                    std::make_shared<ChildTestTypeA>(9654.321465),
                                                                    std::make_shared<ChildTestTypeB>(64532.898323, false)
                                                                   };

        json serialised = esd::Serialise(original);

        REQUIRE(esd::Validate<decltype(original)>(serialised));

        auto deserialised = esd::DeserialiseWithoutChecks<decltype(original)>(serialised);
        json deserialisedReserialised = esd::Serialise(deserialised);

        REQUIRE(original.size() == deserialised.size());
        for (std::size_t i = 0; i < original.size(); ++i) {
            auto& originalPtr = original[i];
            auto& deserialisedPtr = deserialised[i];

            auto objectDataKey = esd::Serialiser<std::shared_ptr<BaseTestType>>::wrappedTypeKey;
            // test this first so failures print out the "before and after" JSON, instead of a less helpful failed comparison
            REQUIRE(deserialisedReserialised.at(i).at(objectDataKey) == serialised.at(i).at(objectDataKey));
            REQUIRE(*deserialisedPtr == *originalPtr);
        }
    }
}
