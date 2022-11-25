#ifndef EASYSERDESJSONHELPERS_H
#define EASYSERDESJSONHELPERS_H

#include "Core.h"

#include <nlohmann/json.hpp>

#include <regex>

/**
 * This file contains code specific to the underlying JSON implementation
 */

namespace esd {

/**
 * A helper function for validating JSON content, allows parsed values of more
 * constrained numeric types to be treated as less constrained types
 */
[[ nodiscard ]] inline constexpr bool MatchType(nlohmann::json::value_t target, nlohmann::json::value_t toMatch)
{
    return target == toMatch
            || (target == nlohmann::json::value_t::number_float && (toMatch == nlohmann::json::value_t::number_integer || toMatch == nlohmann::json::value_t::number_unsigned))
            || (target == nlohmann::json::value_t::number_integer && toMatch == nlohmann::json::value_t::number_unsigned);
}

/**
 * Nlohmann JSON has a built in way to suport serialisation and deserialisation
 * of custom types, the reason for this library is to make it less work and
 * less error prone to do the same job, however it feels important to support it
 * for completeness.
 */
template <typename T>
concept SupportsNlohmannJsonSerialisation = requires (const nlohmann::json& cj, const T& ct, nlohmann::json& j, T& t) {
    { to_json(j, ct) } -> std::same_as<void>;
    { from_json(cj, t) } -> std::same_as<void>;
};

template <SupportsNlohmannJsonSerialisation T>
class Serialiser<T> {
public:
    static bool Validate(const nlohmann::json&)
    {
        return true;
    }

    static nlohmann::json Serialise(const T& value)
    {
        nlohmann::json serialised;
        to_json(serialised, value);
        return serialised;
    }

    static T Deserialise(const nlohmann::json& serialised)
    {
        T value;
        from_json(serialised, value);
        return value;
    }
};

} // end namespace esd

#endif // EASYSERDESJSONHELPERS_H
