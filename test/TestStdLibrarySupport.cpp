#include <EasySerDes.h>

#include <catch2/catch.hpp>

#include <string>
#include <vector>
#include <set>
#include <deque>
#include <tuple>
#include <map>
#include <bitset>
#include <array>
#include <optional>

using namespace nlohmann;

namespace {
template <typename T>
void RunTest(T value, json::value_t desiredStorageType = json::value_t::null)
{
    if constexpr (esd::TypeSupportedByEasySerDes<T>) {
        auto serialised = esd::Serialise(value);
        T deserialised = esd::DeserialiseWithoutChecks<T>(serialised);
        auto reSerialised = esd::Serialise(deserialised);

        // Check this first so that an error displays nicely comparable json structures
        if constexpr (!esd::is_specialization_of<T, std::shared_ptr>::value) {
            // Don't check for shared_ptr because it stores meta data for ptr sharedness
            REQUIRE(serialised == reSerialised);
        }
        if constexpr (esd::is_specialization_of<T, std::shared_ptr>::value || esd::is_specialization_of<T, std::unique_ptr>::value) {
            REQUIRE(*deserialised == *value);
        } else {
            REQUIRE(deserialised == value);
        }
        REQUIRE(esd::Validate<T>(serialised));
        REQUIRE(esd::Validate<T>(reSerialised));

        // null type indicates we don't care what the storage type is
        if (desiredStorageType != json::value_t::null) {
            REQUIRE(serialised.type() == desiredStorageType);
        }
    } else {
        bool typeNotSupported = true;
        REQUIRE(typeNotSupported);
    }
}
} // end anonymous namespace

TEST_CASE("StdLibTypes", "[json]")
{
    SECTION("std::string")
    {
        RunTest<std::string>("Hello World!", json::value_t::string);
    }

    SECTION("std::vector<char>")
    {
        RunTest<std::vector<char>>({ 'H', 'W', '!', '4', '2' }, json::value_t::string);
    }

    SECTION("std::deque<int>")
    {
        RunTest<std::deque<int>>({ 42, 79, 54326781, -541786, 0, -0, ~0 }, json::value_t::array);
    }

    SECTION("std::vector<int>")
    {
        RunTest<std::vector<int>>({ 42, 79, 54326781, -541786, 0, -0, ~0 }, json::value_t::array);
    }

    SECTION("std::set<int>")
    {
        RunTest<std::set<int>>({ 42, 79, 54326781, -541786, 0, -0, ~0 }, json::value_t::array);
    }

    SECTION("std::array<int, 5>")
    {
        RunTest<std::array<char, 5>>({ 1, 2, 3, 4, 5 }, json::value_t::array);
    }

    SECTION("std::bitset")
    {
        RunTest<std::bitset<5>>({ std::bitset<5>{"10110"} }, json::value_t::array);
    }

    SECTION("std::vector<std::string>")
    {
        RunTest<std::vector<std::string>>({ "Hello", " ", "World", "!", "\n", "\t" }, json::value_t::array);
    }

    SECTION("std::pair")
    {
        RunTest(std::make_pair(std::string{"Hello World!"}, std::byte{0xA4}), json::value_t::object);
        RunTest(std::make_pair(std::string{"Hello World!"}, std::make_pair(std::byte{0xA4}, 0.43278f)), json::value_t::object);
    }

    SECTION("std::map<T1, T2>")
    {
        RunTest<std::map<int, bool>>({ {42, true}, {44, false}, {79, true} }, json::value_t::array);
        RunTest<std::map<int, std::vector<int>>>({ {42, std::vector<int>{1,2,3,4,5}}, {44, std::vector<int>{1,2,3,4,5}}, {79, std::vector<int>{1,2,3,4,5}} }, json::value_t::array);
        RunTest<std::map<bool, std::string>>({ {true, "foo"}, {false, "bar"} }, json::value_t::array);
        RunTest<std::map<std::string, bool>>({ {"foo", true}, {"bar", false}, {"foobar", true} }, json::value_t::array);
    }

    SECTION("std::byte")
    {
        RunTest<std::byte>(std::byte{ 0b0101'0101 }, json::value_t::string);
        RunTest<std::byte>(std::byte{ 0b1010'1010 }, json::value_t::string);
        RunTest<std::byte>(std::byte{ 0xFF }, json::value_t::string);
        RunTest<std::byte>(std::byte{ 0x00 }, json::value_t::string);
    }

    SECTION("std::tuple")
    {
        RunTest(std::make_tuple(33, std::string{"Hello World!"}, std::byte{0xA4}), json::value_t::object);
        RunTest(std::make_tuple(std::vector<char>{ 'F', 'o', 'o' }, std::string{"Hello World!"}, std::map<int, int>{ {42, 1}, {79, 44} }), json::value_t::object);
        RunTest<std::tuple<>>(std::make_tuple(), json::value_t::object);
    }

    SECTION("std::optional")
    {
        RunTest(std::make_optional(543), json::value_t::object);
        RunTest(std::make_optional(std::string("FooBar")), json::value_t::object);
        RunTest(std::nullopt, json::value_t::object);
    }

    SECTION("std::shared_ptr")
    {
        RunTest(std::make_shared<int>(42), json::value_t::object);
        RunTest(std::make_shared<std::tuple<int, char, double>>(42, 'f', 0.314), json::value_t::object);

        SECTION("Preserve sharedness")
        {
            auto sharedPtr1 = std::make_shared<int>(42);
            auto sharedPtr2 = sharedPtr1;

            auto serialised1 = esd::Serialise(sharedPtr1);
            auto serialised2 = esd::Serialise(sharedPtr2);

            REQUIRE(serialised1 == serialised2);

            auto deserialisedSharedPtr1 = esd::DeserialiseWithoutChecks<decltype(sharedPtr1)>(serialised1);
            auto deserialisedSharedPtr2 = esd::DeserialiseWithoutChecks<decltype(sharedPtr2)>(serialised2);

            /*
             * It is unreasonable to require that sharedness is preserved between
             * a pointer that was serialised, and a pointer that was created by
             * deserialisation. In most cases significant time will have passed
             * between these two acts, and any number of changes may have
             * occurred to the origional value.
             *
             * This could theoretically be overcome by testing equality between
             * the boxed values would add an uneccesary constraint to every type
             * that was supported, i.e. that it implemented operator==.
             */
            REQUIRE(sharedPtr1 == sharedPtr2);
            REQUIRE(sharedPtr1 != deserialisedSharedPtr1);
            REQUIRE(sharedPtr2 != deserialisedSharedPtr2);
            REQUIRE(deserialisedSharedPtr1 == deserialisedSharedPtr2);
        }

        SECTION("Don't preserve sharedness between cache clears")
        {
            auto sharedPtr1 = std::make_shared<int>(42);
            auto sharedPtr2 = sharedPtr1;

            auto serialised1 = esd::Serialise(sharedPtr1);
            auto serialised2 = esd::Serialise(sharedPtr2);

            REQUIRE(serialised1 == serialised2);

            auto deserialisedSharedPtr1 = esd::DeserialiseWithoutChecks<decltype(sharedPtr1)>(serialised1);
            esd::CacheManager::ClearCaches();
            auto deserialisedSharedPtr2 = esd::DeserialiseWithoutChecks<decltype(sharedPtr2)>(serialised2);

            REQUIRE(sharedPtr1 == sharedPtr2);
            REQUIRE(sharedPtr1 != deserialisedSharedPtr1);
            REQUIRE(sharedPtr2 != deserialisedSharedPtr2);
            REQUIRE(*sharedPtr1 == *deserialisedSharedPtr1);
            REQUIRE(*sharedPtr2 == *deserialisedSharedPtr2);
            REQUIRE(deserialisedSharedPtr1 != deserialisedSharedPtr2);
            REQUIRE(*deserialisedSharedPtr1 == *deserialisedSharedPtr2);
        }

        SECTION("Don't add sharedness to identical values")
        {
            auto sharedPtr1 = std::make_shared<int>(42);
            auto sharedPtr2 = std::make_shared<int>(42);


            auto serialised1 = esd::Serialise(sharedPtr1);
            auto serialised2 = esd::Serialise(sharedPtr2);

            REQUIRE(serialised1 != serialised2);

            auto deserialisedSharedPtr1 = esd::DeserialiseWithoutChecks<decltype(sharedPtr1)>(serialised1);
            auto deserialisedSharedPtr2 = esd::DeserialiseWithoutChecks<decltype(sharedPtr2)>(serialised2);

            REQUIRE(sharedPtr1 != deserialisedSharedPtr1);
            REQUIRE(sharedPtr2 != deserialisedSharedPtr2);
            REQUIRE(deserialisedSharedPtr1 != deserialisedSharedPtr2);

            REQUIRE(*sharedPtr1 == *deserialisedSharedPtr1);
            REQUIRE(*sharedPtr2 == *deserialisedSharedPtr2);
            REQUIRE(*deserialisedSharedPtr1 == *deserialisedSharedPtr2);

            REQUIRE(*sharedPtr1 == *sharedPtr1);
            REQUIRE(*deserialisedSharedPtr1 == *deserialisedSharedPtr2);
        }

        SECTION("Don't extend lifetime of any pointers")
        {
            int testValue = 56743874;

            std::weak_ptr<int> weakPtr;
            nlohmann::json serialised;
            {
                auto shared = std::make_shared<int>(testValue);
                serialised = esd::Serialise(shared);
                weakPtr = shared;
            }

            REQUIRE(esd::Validate<std::shared_ptr<int>>(serialised));

            auto deserialised = esd::DeserialiseWithoutChecks<std::shared_ptr<int>>(serialised);

            REQUIRE(*deserialised == testValue);
            REQUIRE(weakPtr.expired());
        }

        SECTION("Don't maintain sharedness for values that have been modified")
        {
            std::string value = "Correct";

            auto sharedPtr1 = std::make_shared<std::string>(value);
            auto sharedPtr2 = sharedPtr1;

            auto serialised1 = esd::Serialise(sharedPtr1);
            auto serialised2 = esd::Serialise(sharedPtr2);

            REQUIRE(serialised1 == serialised2);

            for (auto& json : serialised2) {
                if (json.is_string() && json.get<std::string>() == value) {
                    json = "Incorrect";
                }
            }

            REQUIRE(serialised1 != serialised2);

            auto deserialisedSharedPtr1 = esd::DeserialiseWithoutChecks<decltype(sharedPtr1)>(serialised1);
            auto deserialisedSharedPtr2 = esd::DeserialiseWithoutChecks<decltype(sharedPtr2)>(serialised2);

            REQUIRE(sharedPtr1 != deserialisedSharedPtr1);
            REQUIRE(*sharedPtr1 == *deserialisedSharedPtr1);
            REQUIRE(sharedPtr2 != deserialisedSharedPtr2);
            REQUIRE(deserialisedSharedPtr1 != deserialisedSharedPtr2);
        }
    }

    SECTION("std::unique_ptr")
    {
        RunTest(std::make_unique<int>(42));
        RunTest(std::make_unique<std::tuple<int, char, double>>(42, 'f', 0.314));
    }
}
