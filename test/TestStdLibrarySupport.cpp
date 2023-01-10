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
#include <variant>
#include <memory>

using namespace nlohmann;

namespace {

// Useful to tell if typename T is an instance of a particular template, usage e.g. IsInstance<T, std::vector>
// https://stackoverflow.com/questions/54182239/c-concepts-checking-for-template-instantiation/54191646#54191646
template <typename T, template <typename...> class Z>
struct is_specialization_of : std::false_type {};
template <typename... Args, template <typename...> class Z>
struct is_specialization_of<Z<Args...>, Z> : std::true_type {};
template <typename T, template <typename...> class Z>
concept IsSpecialisationOf = is_specialization_of<T, Z>::value;

template <typename T>
void RunTest(T value, json::value_t desiredStorageType = json::value_t::null)
{
    esd::Context c;

    if constexpr (esd::TypeSupportedByEasySerDes<T>) {
        auto serialised = esd::Serialise(c, value);
        T deserialised = esd::DeserialiseWithoutChecks<T>(c, serialised);
        auto reSerialised = esd::Serialise(c, deserialised);

        // Check this first so that an error displays nicely comparable json structures
        if constexpr (!IsSpecialisationOf<T, std::shared_ptr>) {
            // Serialiser for shared_ptr stores meta data for ptr sharedness which will not be the same between serialisations
            REQUIRE(serialised == reSerialised);
        } else {
            // The part that should be the same though is contained within a sub-object
            REQUIRE(serialised.at("wrappedType") == reSerialised.at("wrappedType"));
        }
        // If a pointer type, we want to compare the values, not the pointer itself
        if constexpr (IsSpecialisationOf<T, std::shared_ptr> || IsSpecialisationOf<T, std::unique_ptr>) {
            if (value != nullptr) {
                REQUIRE(deserialised != nullptr);
                REQUIRE(*deserialised == *value);
            }
        } else {
            REQUIRE(deserialised == value);
        }
        REQUIRE(esd::Validate<T>(c, serialised));
        REQUIRE(esd::Validate<T>(c, reSerialised));

        // null type indicates we don't care what the storage type is
        if (desiredStorageType != json::value_t::null) {
            REQUIRE(serialised.type() == desiredStorageType);
        }
    } else {
        bool typeSupported = false;
        REQUIRE(typeSupported);
    }
}

template <typename T, typename InvalidType>
requires std::same_as<InvalidType, nlohmann::json> || requires (InvalidType t) { { nlohmann::json(t) }; }
void RunFailureTest(InvalidType invalidValue)
{
    esd::Context c;

    REQUIRE(!esd::Validate<T>(c, invalidValue));
    REQUIRE(esd::Deserialise<T>(c, invalidValue) == std::nullopt);
}

template <typename T, typename... InvalidValueTs>
requires (... && (std::same_as<InvalidValueTs, nlohmann::json> || requires (InvalidValueTs t) { { nlohmann::json(t) }; } ))
void RunFailureTest(InvalidValueTs... invalidValues)
{
    (RunFailureTest<T>(invalidValues), ...);
}

} // end anonymous namespace

TEST_CASE("StdLibTypes", "[json]")
{
    SECTION("std::string")
    {
        RunTest<std::string>("Hello World!", json::value_t::string);
        RunFailureTest<std::string>(14, true, 432.2346, -513, 54378u, std::vector<std::string>{ "Hello World!" });
    }

    SECTION("std::vector<char>")
    {
        RunTest<std::vector<char>>({ 'H', 'W', '!', '4', '2' }, json::value_t::string);
        RunFailureTest<std::vector<char>>(14, true, 432.2346, -513, 54378u, std::vector<int>{ 43, -123, 543, -1435, 54 });
    }

    SECTION("std::deque<int>")
    {
        RunTest<std::deque<int>>({ 42, 79, 54326781, -541786, 0, -0, ~0 }, json::value_t::array);
        RunFailureTest<std::deque<int>>(14, true, 432.2346, -513, 54378u, std::vector<float>{ 43.4532, -123.6432, 543.7832, -1435.005, 54.1 });
    }

    SECTION("std::vector<int>")
    {
        RunTest<std::vector<int>>({ 42, 79, 54326781, -541786, 0, -0, ~0 }, json::value_t::array);
        RunFailureTest<std::vector<int>>(14, true, 432.2346, -513, 54378u, std::vector<float>{ 43.4532, -123.6432, 543.7832, -1435.005, 54.1 });
    }

    SECTION("std::set<int>")
    {
        RunTest<std::set<int>>({ 42, 79, 54326781, -541786, 0, -0, ~0 }, json::value_t::array);
        RunFailureTest<std::set<int>>(14, true, 432.2346, -513, 54378u);
        // ranges all treated generically, so no duplication detection for sets
        // RunFailureTest<std::set<int>>(std::vector<int>{ 1, 1, 543, 2346, 7654, 2346 });
    }

    SECTION("std::array<int, 5>")
    {
        RunTest<std::array<int, 5>>({ 1, 2, 3, 4, 5 }, json::value_t::array);
        RunFailureTest<std::array<int, 5>>(14, true, 432.2346, -513, 54378u, std::array<int, 6>{ 1, 1, 543, 2346, 7654, 2346 });
    }

    SECTION("std::bitset")
    {
        RunTest<std::bitset<5>>({ std::bitset<5>{"10110"} }, json::value_t::string);
        RunTest<std::bitset<15>>({ std::bitset<15>{"10110101011111001100"} }, json::value_t::string);
        RunFailureTest<std::bitset<5>>(14, "011010001", "78463287", "10210", true, 432.2346, -513, 54378u, std::array<int, 6>{ 1, 1, 543, 2346, 7654, 2346 });
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
        RunFailureTest<std::byte>(0u, 256u, "00FF", "1xFF", "0x7C3", "014F");
    }

    SECTION("std::tuple")
    {
        RunTest(std::make_tuple(33, std::string{"Hello World!"}, std::byte{0xA4}), json::value_t::object);
        RunTest(std::make_tuple(std::vector<char>{ 'F', 'o', 'o' }, std::string{"Hello World!"}, std::map<int, int>{ {42, 1}, {79, 44} }), json::value_t::object);
        RunTest<std::tuple<>>(std::make_tuple(), json::value_t::object);
    }

    SECTION("std::optional")
    {
        RunTest(std::make_optional(543), json::value_t::number_integer);
        RunTest(std::make_optional(std::string("FooBar")), json::value_t::string);
        RunTest<std::optional<int>>(std::nullopt, json::value_t::string);
    }

    SECTION("std::variant")
    {
        RunTest<std::variant<int, bool, std::string>>(543, json::value_t::object);
        RunTest<std::variant<int, bool, std::string>>(std::string("FooBar"), json::value_t::object);
        RunTest<std::variant<int, bool, std::string>>(true, json::value_t::object);
        RunFailureTest<std::variant<int, bool, std::string>>(42.12345, 12345.432f);
    }

    SECTION("std::shared_ptr")
    {
        RunTest(std::make_shared<int>(42), json::value_t::object);
        RunTest(std::make_shared<std::tuple<int, char, double>>(42, 'f', 0.314), json::value_t::object);
        RunTest(std::shared_ptr<int>(nullptr), json::value_t::object);

        SECTION("Preserve sharedness within a single Deserialise call")
        {
            auto pointers = std::make_pair(std::make_shared<int>(42), std::make_shared<int>(42));
            REQUIRE(pointers.first != pointers.second);

            esd::Context c;
            auto serialised = esd::Serialise(c, pointers);
            auto deserialisedPtrs = esd::DeserialiseWithoutChecks<decltype(pointers)>(c, serialised);

            REQUIRE(pointers != deserialisedPtrs);

            // pointers are different to the original ones
            REQUIRE(pointers.first.get() != deserialisedPtrs.first.get());
            REQUIRE(pointers.second.get() != deserialisedPtrs.second.get());

            // values are the same as the original ones
            REQUIRE(*pointers.first == *deserialisedPtrs.first);
            REQUIRE(*pointers.second == *deserialisedPtrs.second);
        }

        SECTION("Don't preserve sharedness between seperate calls with different contexts")
        {
            auto sharedPtr1 = std::make_shared<int>(42);
            auto sharedPtr2 = sharedPtr1;

            REQUIRE(reinterpret_cast<uintptr_t>(sharedPtr1.get()) == reinterpret_cast<uintptr_t>(sharedPtr2.get()));

            esd::Context c1;
            auto serialised1 = esd::Serialise(c1, sharedPtr1);
            esd::Context c2;
            auto serialised2 = esd::Serialise(c2, sharedPtr2);

            REQUIRE(serialised1 == serialised2);

            auto deserialisedSharedPtr1 = esd::DeserialiseWithoutChecks<decltype(sharedPtr1)>(c1, serialised1);
            auto deserialisedSharedPtr2 = esd::DeserialiseWithoutChecks<decltype(sharedPtr2)>(c2, serialised2);

            REQUIRE(sharedPtr1 == sharedPtr2);
            REQUIRE(sharedPtr1 != deserialisedSharedPtr1);
            REQUIRE(sharedPtr2 != deserialisedSharedPtr2);
            REQUIRE(deserialisedSharedPtr1 != deserialisedSharedPtr2);
        }


        SECTION("Don't preserve sharedness between seperate calls with no context specified")
        {
            auto sharedPtr1 = std::make_shared<int>(42);
            auto sharedPtr2 = sharedPtr1;

            REQUIRE(reinterpret_cast<uintptr_t>(sharedPtr1.get()) == reinterpret_cast<uintptr_t>(sharedPtr2.get()));

            auto serialised1 = esd::Serialise(sharedPtr1);
            auto serialised2 = esd::Serialise(sharedPtr2);

            REQUIRE(serialised1 == serialised2);

            auto deserialisedSharedPtr1 = esd::DeserialiseWithoutChecks<decltype(sharedPtr1)>(serialised1);
            auto deserialisedSharedPtr2 = esd::DeserialiseWithoutChecks<decltype(sharedPtr2)>(serialised2);

            REQUIRE(sharedPtr1 == sharedPtr2);
            REQUIRE(sharedPtr1 != deserialisedSharedPtr1);
            REQUIRE(sharedPtr2 != deserialisedSharedPtr2);
            REQUIRE(deserialisedSharedPtr1 != deserialisedSharedPtr2);
        }

        SECTION("Preserve sharedness between seperate calls when the same context is used")
        {
            auto sharedPtr1 = std::make_shared<int>(42);
            auto sharedPtr2 = sharedPtr1;

            REQUIRE(reinterpret_cast<uintptr_t>(sharedPtr1.get()) == reinterpret_cast<uintptr_t>(sharedPtr2.get()));

            esd::Context c;
            auto serialised1 = esd::Serialise(c, sharedPtr1);
            auto serialised2 = esd::Serialise(c, sharedPtr2);

            REQUIRE(serialised1 == serialised2);

            auto deserialisedSharedPtr1 = esd::DeserialiseWithoutChecks<decltype(sharedPtr1)>(c, serialised1);
            auto deserialisedSharedPtr2 = esd::DeserialiseWithoutChecks<decltype(sharedPtr2)>(c, serialised2);

            REQUIRE(sharedPtr1 == sharedPtr2);
            REQUIRE(sharedPtr1 != deserialisedSharedPtr1);
            REQUIRE(sharedPtr2 != deserialisedSharedPtr2);
            REQUIRE(deserialisedSharedPtr1 == deserialisedSharedPtr2);
        }

        SECTION("Don't add sharedness to identical values")
        {
            auto pointers = std::make_pair(std::make_shared<int>(42), std::make_shared<int>(42));

            esd::Context c;
            auto serialised = esd::Serialise(c, pointers);
            auto deserialisedPtrs = esd::DeserialiseWithoutChecks<decltype(pointers)>(c, serialised);

            // Having the same value didn't cause sharing
            REQUIRE(deserialisedPtrs.first.get() != deserialisedPtrs.second.get());
        }

        SECTION("Don't extend lifetime of any pointers")
        {
            int testValue = 56743874;

            esd::Context c;
            std::weak_ptr<int> weakPtr;
            nlohmann::json serialised;
            {
                auto shared = std::make_shared<int>(testValue);
                serialised = esd::Serialise(c, shared);
                weakPtr = shared;
            }

            REQUIRE(esd::Validate<std::shared_ptr<int>>(c, serialised));

            auto deserialised = esd::DeserialiseWithoutChecks<std::shared_ptr<int>>(c, serialised);

            REQUIRE(*deserialised == testValue);
            REQUIRE(weakPtr.expired());
        }

        SECTION("Don't maintain sharedness for values that have been modified")
        {
            std::string value = "Correct";

            auto sharedPtr1 = std::make_shared<std::string>(value);
            auto sharedPtr2 = sharedPtr1;

            esd::Context c;
            auto serialised1 = esd::Serialise(c, sharedPtr1);
            auto serialised2 = esd::Serialise(c, sharedPtr2);

            REQUIRE(serialised1 == serialised2);

            for (auto& json : serialised2) {
                if (json.is_string() && json.get<std::string>() == value) {
                    json = "Incorrect";
                }
            }

            REQUIRE(serialised1 != serialised2);

            auto deserialisedSharedPtr1 = esd::DeserialiseWithoutChecks<decltype(sharedPtr1)>(c, serialised1);
            auto deserialisedSharedPtr2 = esd::DeserialiseWithoutChecks<decltype(sharedPtr2)>(c, serialised2);

            REQUIRE(sharedPtr1 != deserialisedSharedPtr1);
            REQUIRE(*sharedPtr1 == *deserialisedSharedPtr1);
            REQUIRE(sharedPtr2 != deserialisedSharedPtr2);
            REQUIRE(deserialisedSharedPtr1 != deserialisedSharedPtr2);
        }
    }

    SECTION("std::unique_ptr")
    {
        RunTest(std::make_unique<int>(42), json::value_t::number_integer);
        RunTest(std::make_unique<std::tuple<int, char, double>>(42, 'f', 0.314), json::value_t::object);
        RunTest(std::unique_ptr<int>(nullptr), json::value_t::string);
    }
}
