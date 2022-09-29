#include <EasySerDes.h>

#include <catch2/catch.hpp>

#include <string>
#include <vector>
#include <tuple>
#include <map>
#include <bitset>
#include <array>

using namespace nlohmann;

namespace {
template <typename T>
void RunTest(T value, json::value_t desiredStorageType = json::value_t::null)
{
    auto serialised = util::Serialise(value);
    T deserialised = util::DeserialiseWithoutChecks<T>(serialised);
    auto reSerialised = util::Serialise(deserialised);

    // Check this first so that an error displays nicely comparable json structures
    REQUIRE(serialised == reSerialised);
    REQUIRE(deserialised == value);
    REQUIRE(util::Validate<T>(serialised));
    REQUIRE(util::Validate<T>(reSerialised));

    // null type indicates we don't care what the storage type is
    if (desiredStorageType != json::value_t::null) {
        REQUIRE(serialised.type() == desiredStorageType);
    }
}
} // end anonymous namespace

// FIXME add test cases that are expected to fail and return nullopt when deserialising

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

    SECTION("std::vector<int>")
    {
        RunTest<std::vector<int>>({ 42, 79, 54326781, -541786, 0, -0, ~0 }, json::value_t::array);
    }

    // TODO specialisations for these types
//    SECTION("std::array<int, 5>")
//    {
//        RunTest<std::array<char, 5>>({ 1, 2, 3, 4, 5 }, json::value_t::array);
//    }

//    SECTION("std::bitset")
//    {
//        RunTest<std::bitset<5>>({ std::bitset<5>{"10110"} }, json::value_t::array);
//    }

    SECTION("std::vector<std::string>")
    {
        RunTest<std::vector<std::string>>({ "Hello", " ", "World", "!", "\n", "\t" }, json::value_t::array);
    }

    SECTION("std::map<T1, T2>")
    {
        RunTest<std::map<int, bool>>({ {42, true}, {44, false}, {79, true} }, json::value_t::array);
        RunTest<std::map<int, std::vector<int>>>({ {42, std::vector<int>{1,2,3,4,5}}, {44, std::vector<int>{1,2,3,4,5}}, {79, std::vector<int>{1,2,3,4,5}} }, json::value_t::array);
    }

    SECTION("std::byte")
    {
        RunTest<std::byte>(std::byte{ 0b0101'0101 }, json::value_t::string);
        RunTest<std::byte>(std::byte{ 0b1010'1010 }, json::value_t::string);
        RunTest<std::byte>(std::byte{ 0xFF }, json::value_t::string);
        RunTest<std::byte>(std::byte{ 0x00 }, json::value_t::string);
    }

    SECTION("std::pair")
    {
        RunTest(std::make_pair(std::string{"Hello World!"}, std::byte{0xA4}), json::value_t::array);
        RunTest(std::make_pair(std::string{"Hello World!"}, std::make_pair(std::byte{0xA4}, 0.43278f)), json::value_t::array);
    }

    SECTION("std::tuple")
    {
        RunTest(std::make_tuple(33, std::string{"Hello World!"}, std::byte{0xA4}), json::value_t::array);
        RunTest(std::make_tuple(std::vector<char>{ 'F', 'o', 'o' }, std::string{"Hello World!"}, std::map<int, int>{ {42, 1}, {79, 44} }), json::value_t::array);
        RunTest<std::tuple<>>(std::make_tuple(), json::value_t::array);
    }
}
