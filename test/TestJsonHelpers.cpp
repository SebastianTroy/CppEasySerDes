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
                REQUIRE(util::MatchType(targetType, serialisedType));
            } else {
                REQUIRE_FALSE(util::MatchType(targetType, serialisedType));
            }
        }
    }
}
