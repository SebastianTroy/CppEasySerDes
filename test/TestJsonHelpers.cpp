#include "TestJsonHelpers.h"

#include "EasySerDesJsonHelpers.h"

#include <catch2/catch.hpp>

using namespace nlohmann;

TEST_CASE("Test MatchType")
{
    std::vector<json::value_t> allTypes{
        json::value_t::array, json::value_t::boolean, json::value_t::number_float, json::value_t::number_integer, json::value_t::number_unsigned, json::value_t::string,
    };

    for (const auto& targetType : allTypes) {
        for (const auto& serialisedType : allTypes) {
            bool typesAreSame = targetType == serialisedType;
            bool serialisedTypeIsLessConstrainedNumericType = (targetType == json::value_t::number_float && serialisedType == json::value_t::number_integer)
                                                           || (targetType == json::value_t::number_float && serialisedType == json::value_t::number_unsigned)
                                                           || (targetType == json::value_t::number_integer && serialisedType == json::value_t::number_unsigned);
            if (typesAreSame || serialisedTypeIsLessConstrainedNumericType) {
                REQUIRE(esd::MatchType(targetType, serialisedType));
            } else {
                REQUIRE_FALSE(esd::MatchType(targetType, serialisedType));
            }
        }
    }
}

TEST_CASE("CreateRegex")
{
    auto basicChecks = [](auto number)
    {
        using T = decltype(number);
        std::regex regexStr = esd::CreateRegex<T>();
        REQUIRE(std::regex_match("0", regexStr));
        REQUIRE(std::regex_match("1", regexStr));
        REQUIRE(std::regex_match("10", regexStr));
        REQUIRE(std::regex_match("-0", regexStr) == std::is_signed_v<T>);
        REQUIRE(std::regex_match("+0", regexStr) == std::is_signed_v<T>);
        REQUIRE(std::regex_match("-1", regexStr) == std::is_signed_v<T>);
        REQUIRE(std::regex_match("+1", regexStr) == std::is_signed_v<T>);
        REQUIRE(std::regex_match(std::to_string(std::numeric_limits<T>::min()), regexStr));
        REQUIRE(std::regex_match(std::to_string(std::numeric_limits<T>::max()), regexStr));
        REQUIRE(std::regex_match("+" + std::to_string(std::numeric_limits<T>::max()), regexStr) == std::is_signed_v<T>);
        REQUIRE_FALSE(std::regex_match(std::to_string(std::numeric_limits<T>::min()) + "0", regexStr) == std::is_signed_v<T>);
        REQUIRE_FALSE(std::regex_match(std::to_string(std::numeric_limits<T>::max()) + "0", regexStr));
        REQUIRE(std::regex_match(std::to_string(number), regexStr));
    };

    SECTION("short")
    {
        short val = 24345;
        basicChecks(val);



        std::regex regexStr = esd::CreateRegex<short>();
        REQUIRE_FALSE(std::regex_match("+2434553", regexStr));
        REQUIRE_FALSE(std::regex_match("2434553", regexStr));
        REQUIRE_FALSE(std::regex_match("-2434553", regexStr));
    }

    SECTION("int")
    {
        int val = -54324765;
        basicChecks(val);

        std::regex regexStr = esd::CreateRegex<int>();
        REQUIRE_FALSE(std::regex_match("+54324765543254345", regexStr));
        REQUIRE_FALSE(std::regex_match("54324765543254345", regexStr));
        REQUIRE_FALSE(std::regex_match("-54324765543254345", regexStr));
    }

    SECTION("intmax_t")
    {
        intmax_t val = -324765;
        basicChecks(val);

        std::regex regexStr = esd::CreateRegex<intmax_t>();
        REQUIRE_FALSE(std::regex_match("-3247657846576482376457", regexStr));
        REQUIRE_FALSE(std::regex_match("3247657846576482376457", regexStr));
        REQUIRE_FALSE(std::regex_match("+3247657846576482376457", regexStr));
    }

    SECTION("uintmax_t")
    {
        uintmax_t val = 324765;
        basicChecks(val);

        std::regex regexStr = esd::CreateRegex<uintmax_t>();
        REQUIRE_FALSE(std::regex_match("-324765566564873874657483764578", regexStr));
        REQUIRE_FALSE(std::regex_match("324765566564873874657483764578", regexStr));
        REQUIRE_FALSE(std::regex_match("+324765566564873874657483764578", regexStr));
    }

    // FIXME test floating point types
}

TEST_CASE("Test JsonSerialiser<T that SupportsNlohmannJsonSerialisation>")
{
    TypeThatSupportsNlohmannJsonSerialisation t { 42, "Foo" };

    auto serialised = esd::Serialise(t);
    auto deserialised = esd::DeserialiseWithoutChecks<TypeThatSupportsNlohmannJsonSerialisation>(serialised);
    auto reserialised = esd::Serialise(deserialised);

    REQUIRE(serialised == reserialised);
    REQUIRE(t.a_ == deserialised.a_);
    REQUIRE(t.b_ == deserialised.b_);
}
