#ifndef TESTJSONHELPERS_H
#define TESTJSONHELPERS_H

#include <nlohmann/json.hpp>

struct TypeThatSupportsNlohmannJsonSerialisation {
    int a_;
    std::string b_;
};

void to_json(nlohmann::json& j, const TypeThatSupportsNlohmannJsonSerialisation& t)
{
    j = { {"a", t.a_}, {"b", t.b_} };
}

void from_json(const nlohmann::json& j, TypeThatSupportsNlohmannJsonSerialisation& t)
{
    t.a_ = j["a"].get<int>();
    t.b_ = j["b"].get<std::string>();
}


#endif // TESTJSONHELPERS_H
