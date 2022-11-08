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

    template <typename T, typename InvalidType>
    requires std::same_as<InvalidType, nlohmann::json> || requires (InvalidType t) { { nlohmann::json(t) }; }
    void RunFailureTest(InvalidType invalidValue)
    {
        REQUIRE(!esd::Validate<T>(invalidValue));
        REQUIRE(esd::Deserialise<T>(invalidValue) == std::nullopt);
    }

    template <typename T, typename... InvalidValueTs>
    requires (... && (std::same_as<InvalidValueTs, nlohmann::json> || requires (InvalidValueTs t) { { nlohmann::json(t) }; } ))
    void RunFailureTest(InvalidValueTs... invalidValues)
    {
        (RunFailureTest<T>(invalidValues), ...);
    }
} // end anonymous namespace

TEST_CASE("BuiltInTypes", "[json]")
{
    SECTION("bool")
    {
        RunTest<bool>(true, json::value_t::boolean);
        RunTest<bool>(false, json::value_t::boolean);

        RunFailureTest<bool>("true", "false", 0, 0.1, nlohmann::json{ 0, 1, 2, 3}, TestEasySerDesEnum::First, TestEasySerDesEnumClass::Middle);
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

        RunFailureTest<char>(44, static_cast<int>(std::numeric_limits<char>::min()) - 1, static_cast<int>(std::numeric_limits<char>::max()) + 1);
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

        RunFailureTest<unsigned char>(-'c', static_cast<unsigned int>(std::numeric_limits<unsigned char>::min()) - 1, static_cast<unsigned int>(std::numeric_limits<unsigned char>::max()) + 1);
    }

    SECTION("int")
    {
        RunTest<int>(0, json::value_t::number_integer);
        RunTest<int>(-437218, json::value_t::number_integer);
        RunTest<int>(587298567, json::value_t::number_integer);
        RunTest<int>(std::numeric_limits<int>::min(), json::value_t::number_integer);
        RunTest<int>(std::numeric_limits<int>::max(), json::value_t::number_integer);
        RunTest<int>(std::numeric_limits<int>::lowest(), json::value_t::number_integer);

        RunFailureTest<int>("Foo", static_cast<unsigned char>('f'), false, static_cast<long long int>(std::numeric_limits<int>::min()) - 1, static_cast<long long int>(std::numeric_limits<int>::max()) + 1);

    }

    SECTION("uint32_t")
    {
        RunTest<uint32_t>(0, json::value_t::number_unsigned);
        RunTest<uint32_t>(-437218, json::value_t::number_unsigned);
        RunTest<uint32_t>(587298567, json::value_t::number_unsigned);
        RunTest<uint32_t>(std::numeric_limits<int>::min(), json::value_t::number_unsigned);
        RunTest<uint32_t>(std::numeric_limits<int>::max(), json::value_t::number_unsigned);
        RunTest<uint32_t>(std::numeric_limits<int>::lowest(), json::value_t::number_unsigned);

        RunFailureTest<uint32_t>(false, -1, -'h', static_cast<uint64_t>(std::numeric_limits<uint32_t>::max()) + 1, "Foo");
    }

    SECTION("unsigned")
    {
        RunTest<unsigned>(0, json::value_t::number_unsigned);
        RunTest<unsigned>(543728476, json::value_t::number_unsigned);
        RunTest<unsigned>(std::numeric_limits<int>::min(), json::value_t::number_unsigned);
        RunTest<unsigned>(std::numeric_limits<int>::max(), json::value_t::number_unsigned);

        RunFailureTest<unsigned>(false, -1, -'h', -4637278, "Foo");
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

        RunFailureTest<float>(false, 'c', -'h', "Foo", json::parse("[ 6473224456568735.564334542566425626 ]").at(0), json::parse("[ -8572424565634268527.32424568345756 ]").at(0));
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

        RunFailureTest<double>(false, 'c', -'h', "Foo");
        // The following don't fail because the library just slices the values down to valid doubles
        // json::parse("[ 64732244235.567483298574645245664334542543266425626 ]").at(0)
        // json::parse("[ -85724245656342682346527.32234565426424568323445756 ]").at(0)
    }

    SECTION("long double")
    {
        RunTest<long double>(0.0, json::value_t::string);

        // need a single word type to use brace initialisation (which checks that the value is within range of the type at compile time)
        using longDouble = long double;
        RunTest<long double>(longDouble{ 64732244235.567483298574645245664334542543266425626 }, json::value_t::string);
        RunTest<long double>(longDouble{ -85724245656342682346527.32234565426424568323445756 }, json::value_t::string);

        RunTest<long double>(std::numeric_limits<long double>::min(), json::value_t::string);
        RunTest<long double>(std::numeric_limits<long double>::max(), json::value_t::string);
        RunTest<long double>(std::numeric_limits<long double>::lowest(), json::value_t::string);
        RunTest<long double>(std::numeric_limits<long double>::epsilon(), json::value_t::string);
        RunTest<long double>(std::numeric_limits<long double>::denorm_min(), json::value_t::string);
        RunTest<long double>(std::numeric_limits<long double>::round_error(), json::value_t::string);

        RunFailureTest<long double>(false, 'c', -'h', "Foo", json::parse("[ \"647322454235.5674832985746452456643345425432664256260\" ]").at(0), json::parse("[ \"-857242545656342682346527.322345654264245683234457560\" ]").at(0));
    }

    // TODO create an EnumHelper that allows the user to set a max and min value, or set allowed values, or set valid flags etc
    SECTION("enum")
    {
        RunTest<TestEasySerDesEnum>(First);
        RunTest<TestEasySerDesEnum>(Middle);
        RunTest<TestEasySerDesEnum>(Last);
        RunTest<TestEasySerDesEnum>(static_cast<TestEasySerDesEnum>(42));

        RunFailureTest<TestEasySerDesEnum>(false, -1, -4637278, "Foo");
    }

    SECTION("enum class")
    {
        RunTest<TestEasySerDesEnumClass>(TestEasySerDesEnumClass::First);
        RunTest<TestEasySerDesEnumClass>(TestEasySerDesEnumClass::Middle);
        RunTest<TestEasySerDesEnumClass>(TestEasySerDesEnumClass::Last);
        RunTest<TestEasySerDesEnumClass>(TestEasySerDesEnumClass{42});

        RunFailureTest<TestEasySerDesEnumClass>(false, -4637278, "Foo");
    }
}
