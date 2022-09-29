#ifndef EASYSERDESJSONHELPERS_H
#define EASYSERDESJSONHELPERS_H

#include "EasySerDesCore.h"

#include <nlohmann/json.hpp>

/**
 * This file contains code specific to the underlying JSON implementation
 */

namespace util {

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

// FIXME implement a helper that supports any type that implements the "to_json" and "from_json"
//       functions that let types play nice with nlohmann_json

} // end namespace util

#endif // EASYSERDESJSONHELPERS_H
