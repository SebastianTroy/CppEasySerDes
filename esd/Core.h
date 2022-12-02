#ifndef EASYSERDESCORE_H
#define EASYSERDESCORE_H

#include "CurrentContext.h"

#include <nlohmann/json.hpp>

#include <optional>
#include <concepts>
#include <sstream>
#include <iomanip>
#include <regex>
#include <sstream>

/**
 * The minimal API for this library.
 *
 *  - Serialise<T>
 *    Returns a serialised version of an item of type T.
 *
 *  - Validate<T>
 *    Checks a serialised item and returns true if it can be deserialised into
 *    an instance of T.
 *
 *  - Deserialise<T>
 *    Converts a serialised item into an instance of type T and returns it
 *    wrapped in a std::optional<T>. If the serialised item is not valid then it
 *    returns std::nullopt.
 *
 *  - DeserialiseWithoutChecks<T>
 *    Converts a serialised item into an instance of type T. If the serialised
 *    item is not valid then program execution is terminated.
 */

namespace esd {

/**
 * This library uses the type erasure pattern. Specialisations of this type are
 * used to support serialisation and deserialisation of the templated type T.
 *
 * This default template is never defined and so any type which does not have a
 * specialisation will fail to compile.
 */
template <typename T>
class Serialiser;

/**
 * Concept used to constrain the API and give more concise error messages when
 * an unsupported type is used with it.
 */
template <typename T>
concept TypeSupportedByEasySerDes = requires { Serialiser<T>{}; } && std::same_as<T, std::remove_cvref_t<T>>;

///
/// API functions
///

/**
 * Returns true if Deserialise into an instance of T will succeed on the given
 * json.
 *
 * User can pass a ScopedContextReset to prevent esd::CurrentContext being reset
 * at the end of this call.
 */
template <typename T> requires TypeSupportedByEasySerDes<T>
bool Validate(const nlohmann::json& serialised, [[maybe_unused]] const ContextStateLifetime& = {})
{
    return Serialiser<T>::Validate(serialised);
}

/**
 * Converts instance of type T into json
 *
 * User can pass a ScopedContextReset to prevent esd::CurrentContext being reset
 * at the end of this call.
 */
template <typename T> requires TypeSupportedByEasySerDes<T>
nlohmann::json Serialise(const T& value, [[maybe_unused]] const ContextStateLifetime& = {})
{
    return Serialiser<T>::Serialise(value);
}

/**
 * Converts valid json into an instance of std::optional<T> or a std::nullopt if
 * the json is not a valid representation of an instance of type T.
 *
 * User can pass a ScopedContextReset to prevent esd::CurrentContext being reset
 * at the end of this call.
 */
template <typename T> requires TypeSupportedByEasySerDes<T>
std::optional<T> Deserialise(const nlohmann::json& serialised, [[maybe_unused]] const ContextStateLifetime& = {})
{
    if (Serialiser<T>::Validate(serialised)) {
        return std::make_optional(Serialiser<T>::Deserialise(serialised));
    } else {
        return std::nullopt;
    }
}

/**
 * Converts valid json into an instance of type T or if it is invalid,
 * terminates execution of the program.
 *
 * User can pass a ScopedContextReset to prevent esd::CurrentContext being reset
 * at the end of this call.
 */
template <typename T> requires TypeSupportedByEasySerDes<T>
T DeserialiseWithoutChecks(const nlohmann::json& serialised, [[maybe_unused]] const ContextStateLifetime& = {})
{
    return Serialiser<T>::Deserialise(serialised);
}

} // end namespace esd

#endif // EASYSERDESCORE_H
