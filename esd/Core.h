#ifndef EASYSERDESCORE_H
#define EASYSERDESCORE_H

#include "Context.h"
#include "DataWriter.h"

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
concept TypeSupportedByEasySerDes = std::same_as<T, std::remove_cvref_t<T>> && requires (Context& context, const T& t, DataWriter&& writer, const nlohmann::json& dataSource) {
    { Serialiser<T>::Validate(context, dataSource) } -> std::same_as<bool>;
    { Serialiser<T>::Serialise(std::move(writer), t) } -> std::same_as<void>;
    { Serialiser<T>::Deserialise(context, dataSource) } -> std::same_as<T>;
};

// Final target
// template <typename T>
// concept TypeSupportedByEasySerDes = std::same_as<T, std::remove_cvref_t<T>> && requires (DataReader&& reader, DataWriter&& writer, const T& t) {
//     { Serialiser<T>::Serialise(writer, t) } -> std::same_as<void>;
//     { Serialiser<T>::Deserialise(reader) } -> std::same_as<T>;
// };

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
bool Validate(Context& context, const nlohmann::json& serialised)
{
    return Serialiser<T>::Validate(context, serialised);
}

/**
 * Converts instance of type T into json
 *
 * User can pass a ScopedContextReset to prevent esd::CurrentContext being reset
 * at the end of this call.
 */
template <typename T> requires TypeSupportedByEasySerDes<T>
nlohmann::json Serialise(Context& context, const T& value)
{
    nlohmann::json data;
    Serialiser<T>::Serialise(DataWriter::Create(context, "Serialise " + detail::TypeNameStr<T>(), data), value);
    return data;
}

/**
 * Converts valid json into an instance of std::optional<T> or a std::nullopt if
 * the json is not a valid representation of an instance of type T.
 *
 * User can pass a ScopedContextReset to prevent esd::CurrentContext being reset
 * at the end of this call.
 */
template <typename T> requires TypeSupportedByEasySerDes<T>
std::optional<T> Deserialise(Context& context, const nlohmann::json& serialised)
{
    if (Serialiser<T>::Validate(context, serialised)) {
        return std::make_optional(Serialiser<T>::Deserialise(context, serialised));
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
T DeserialiseWithoutChecks(Context& context, const nlohmann::json& serialised)
{
    return Serialiser<T>::Deserialise(context, serialised);
}

template <typename T> requires TypeSupportedByEasySerDes<T>
bool Validate(const nlohmann::json& serialised)
{
    Context context;
    return Validate<T>(context, serialised);
}

template <typename T> requires TypeSupportedByEasySerDes<T>
nlohmann::json Serialise(const T& value)
{
    Context context;
    return Serialise<T>(context, value);
}

template <typename T> requires TypeSupportedByEasySerDes<T>
std::optional<T> Deserialise(const nlohmann::json& serialised)
{
    Context context;
    return Deserialise<T>(context, serialised);
}

template <typename T> requires TypeSupportedByEasySerDes<T>
T DeserialiseWithoutChecks(const nlohmann::json& serialised)
{
    Context context;
    return DeserialiseWithoutChecks<T>(context, serialised);
}

} // end namespace esd

#endif // EASYSERDESCORE_H
