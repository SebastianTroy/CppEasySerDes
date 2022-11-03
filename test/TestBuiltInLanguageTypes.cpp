#include "TestBuiltInLanguageTypes.h"

#include <EasySerDes.h>

#include <catch2/catch.hpp>

#include <limits>

using namespace nlohmann;

namespace {
    template <typename T>
    void RunTest(T value, json::value_t desiredStorageType = json::value_t::null)
    {
        auto serialised = esd::Serialise<T>(value);
        T deserialised = esd::DeserialiseWithoutChecks<T>(serialised);
        auto reSerialised = esd::Serialise<T>(deserialised);

        // Check this first so that an error displays nicely comparable json structures
        REQUIRE(serialised == reSerialised);
        REQUIRE(deserialised == value);
        REQUIRE(esd::Validate<T>(serialised));
        REQUIRE(esd::Validate<T>(reSerialised));

        // null type indicates we don't care what the storage type is
        if (desiredStorageType != json::value_t::null) {
            REQUIRE(serialised.type() == desiredStorageType);
        }
    }
} // end anonymous namespace

// FIXME add test cases for validation of values too large to fit into type
// FIXME add test cases that are expected to fail and return nullopt when deserialising
// FIXME add test cases where hand crafted json with errors is parsed
//       -- handling of syntax errors etc, does trhe program fail or cope as expected
//       -- handling of types that are different but compatible (i.e. an int value stored for a floating point type etc)


TEST_CASE("BuiltInTypes", "[json]")
{
    SECTION("bool")
    {
        RunTest<bool>(true, json::value_t::boolean);
        RunTest<bool>(false, json::value_t::boolean);
    }

    SECTION("char")
    {
        RunTest<char>('a', json::value_t::string);
        RunTest<char>('9', json::value_t::string);
        RunTest<char>('#', json::value_t::string);
        RunTest<char>('\n', json::value_t::string);
        RunTest<char>('\0', json::value_t::string);
        RunTest<char>(std::numeric_limits<char>::min(), json::value_t::string);
        RunTest<char>(std::numeric_limits<char>::max(), json::value_t::string);
        RunTest<char>(std::numeric_limits<char>::lowest(), json::value_t::string);
    }

    SECTION("unsigned char")
    {
        RunTest<unsigned char>('a');
        RunTest<unsigned char>('9');
        RunTest<unsigned char>('#');
        RunTest<unsigned char>('\n');
        RunTest<unsigned char>('\0');
        RunTest<unsigned char>(std::numeric_limits<unsigned char>::min());
        RunTest<unsigned char>(std::numeric_limits<unsigned char>::max());
        RunTest<unsigned char>(std::numeric_limits<unsigned char>::lowest());
    }

    SECTION("int")
    {
        RunTest<int>(0, json::value_t::number_integer);
        RunTest<int>(-437218, json::value_t::number_integer);
        RunTest<int>(587298567, json::value_t::number_integer);
        RunTest<int>(std::numeric_limits<int>::min(), json::value_t::number_integer);
        RunTest<int>(std::numeric_limits<int>::max(), json::value_t::number_integer);
        RunTest<int>(std::numeric_limits<int>::lowest(), json::value_t::number_integer);
    }

    SECTION("uint32_t")
    {
        RunTest<uint32_t>(0, json::value_t::number_unsigned);
        RunTest<uint32_t>(-437218, json::value_t::number_unsigned);
        RunTest<uint32_t>(587298567, json::value_t::number_unsigned);
        RunTest<uint32_t>(std::numeric_limits<int>::min(), json::value_t::number_unsigned);
        RunTest<uint32_t>(std::numeric_limits<int>::max(), json::value_t::number_unsigned);
        RunTest<uint32_t>(std::numeric_limits<int>::lowest(), json::value_t::number_unsigned);
    }

    SECTION("unsigned")
    {
        RunTest<unsigned>(0, json::value_t::number_unsigned);
        RunTest<unsigned>(543728476, json::value_t::number_unsigned);
        RunTest<unsigned>(std::numeric_limits<int>::min(), json::value_t::number_unsigned);
        RunTest<unsigned>(std::numeric_limits<int>::max(), json::value_t::number_unsigned);
    }

    SECTION("float")
    {
        RunTest<float>(0.0f, json::value_t::number_float);
        RunTest<float>(647328735.564326f, json::value_t::number_float);
        RunTest<float>(-85734268527.3248756f, json::value_t::number_float);
        RunTest<float>(std::numeric_limits<float>::min(), json::value_t::number_float);
        RunTest<float>(std::numeric_limits<float>::max(), json::value_t::number_float);
        RunTest<float>(std::numeric_limits<float>::lowest(), json::value_t::number_float);
        RunTest<float>(std::numeric_limits<float>::epsilon(), json::value_t::number_float);
        RunTest<float>(std::numeric_limits<float>::denorm_min(), json::value_t::number_float);
        RunTest<float>(std::numeric_limits<float>::round_error(), json::value_t::number_float);
    }

    SECTION("double")
    {
        RunTest<double>(0.0, json::value_t::number_float);
        RunTest<double>(double{6473224456568735.564334542566425626}, json::value_t::number_float);
        RunTest<double>(double{-8572424565634268527.32424568345756}, json::value_t::number_float);
        RunTest<double>(std::numeric_limits<double>::min(), json::value_t::number_float);
        RunTest<double>(std::numeric_limits<double>::max(), json::value_t::number_float);
        RunTest<double>(std::numeric_limits<double>::lowest(), json::value_t::number_float);
        RunTest<double>(std::numeric_limits<double>::epsilon(), json::value_t::number_float);
        RunTest<double>(std::numeric_limits<double>::denorm_min(), json::value_t::number_float);
        RunTest<double>(std::numeric_limits<double>::round_error(), json::value_t::number_float);
    }

    SECTION("long double")
    {
        RunTest<long double>(0.0, json::value_t::string);

        // need a single word type to use brace initialisation (which checks that the value is within range of the type at compile time)
        using longDouble = long double;
        RunTest<long double>(longDouble{64732244235.567483298574645245664334542543266425626}, json::value_t::string);
        RunTest<long double>(longDouble{-85724245656342682346527.32234565426424568323445756}, json::value_t::string);

        RunTest<long double>(std::numeric_limits<long double>::min(), json::value_t::string);
        RunTest<long double>(std::numeric_limits<long double>::max(), json::value_t::string);
        RunTest<long double>(std::numeric_limits<long double>::lowest(), json::value_t::string);
        RunTest<long double>(std::numeric_limits<long double>::epsilon(), json::value_t::string);
        RunTest<long double>(std::numeric_limits<long double>::denorm_min(), json::value_t::string);
        RunTest<long double>(std::numeric_limits<long double>::round_error(), json::value_t::string);
    }

    SECTION("enum")
    {
        RunTest<TestEasySerDesEnum>(First);
        RunTest<TestEasySerDesEnum>(Middle);
        RunTest<TestEasySerDesEnum>(Last);
        RunTest<TestEasySerDesEnum>(static_cast<TestEasySerDesEnum>(42));
    }

    SECTION("enum class")
    {
        RunTest<TestEasySerDesEnumClass>(TestEasySerDesEnumClass::First);
        RunTest<TestEasySerDesEnumClass>(TestEasySerDesEnumClass::Middle);
        RunTest<TestEasySerDesEnumClass>(TestEasySerDesEnumClass::Last);
        RunTest<TestEasySerDesEnumClass>(TestEasySerDesEnumClass{42});
    }
}
