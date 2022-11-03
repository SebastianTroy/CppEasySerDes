#ifndef TESTBUILTINLANGUAGETYPES_H
#define TESTBUILTINLANGUAGETYPES_H

#include <nlohmann/json.hpp>

#include <iostream>

/*
 * Some Types for testing
 */

enum TestEasySerDesEnum {
    First,
    Middle,
    Last,
};

enum class TestEasySerDesEnumClass : short {
    First,
    Middle,
    Last,
};

/*
 * Some operators to make testing easier
 */

std::ostream& operator<<(std::ostream& ostr, const TestEasySerDesEnum& value)
{
    switch (value) {
        case TestEasySerDesEnum::First : return ostr << "TestEasySerDesEnum::First";
        case TestEasySerDesEnum::Middle : return ostr << "TestEasySerDesEnum::Middle";
        case TestEasySerDesEnum::Last : return ostr << "TestEasySerDesEnum::Last";
    }
    return ostr << "TestEnum::INVALID";
}

std::ostream& operator<<(std::ostream& ostr, const TestEasySerDesEnumClass& value)
{
    switch (value) {
    case TestEasySerDesEnumClass::First : return ostr << "TestEasySerDesEnumClass::First";
    case TestEasySerDesEnumClass::Middle : return ostr << "TestEasySerDesEnumClass::Middle";
    case TestEasySerDesEnumClass::Last : return ostr << "TestEasySerDesEnumClass::Last";
    }
    return ostr << "TestEnum::INVALID";
}

std::ostream& operator<<(std::ostream& ostr, const nlohmann::json::value_t& type)
{
    switch (type) {
    case nlohmann::json::value_t::null : return ostr << "null";
    case nlohmann::json::value_t::object : return ostr << "object";
    case nlohmann::json::value_t::array : return ostr << "array";
    case nlohmann::json::value_t::string : return ostr << "string";
    case nlohmann::json::value_t::boolean : return ostr << "bool";
    case nlohmann::json::value_t::number_integer : return ostr << "int";
    case nlohmann::json::value_t::number_unsigned : return ostr << "unsigned";
    case nlohmann::json::value_t::number_float : return ostr << "double";
    case nlohmann::json::value_t::binary : return ostr << "binary";
    case nlohmann::json::value_t::discarded : return ostr << "discarded";
    }
    return ostr << "INVALID ENUM VALUE FOR JSON::VALUE_T";
}
#endif // TESTBUILTINLANGUAGETYPES_H
