#ifndef EASYSERDESCORE_H
#define EASYSERDESCORE_H

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
class JsonSerialiser;

/**
 * Anything that satisfies this concept is supported by this library.
 *
 * However there is a lot of boilerplate and code repetition required for
 * supporting your own types this way, it is mainly used to support built in
 * language types.
 */
template <typename T>
concept JsonSerialserConcept = requires (const T& t, const nlohmann::json& s) {
    { T::Validate(s) } -> std::same_as<bool>;
    { T::Serialise(t) } -> std::same_as<nlohmann::json>;
    { T::Deserialise(s) } -> std::same_as<T>;
};

/**
 * Concept used to constrain the API and give more concise error messages when
 * an unsupported type is used with it.
 */
template <typename T>
concept TypeSupportedByEasySerDes = requires { JsonSerialiser<T>{}; } && std::same_as<T, std::remove_cvref_t<T>>;

///
/// Forward Declare API functions
///

/**
 * Returns true if Deserialise into an instance of T will succeed on the given
 * json.
 */
template <typename T> requires TypeSupportedByEasySerDes<T>
bool Validate(const nlohmann::json& serialised);

/**
 * Converts instance of type T into json
 */
template <typename T> requires TypeSupportedByEasySerDes<T>
nlohmann::json Serialise(const T& value);

/**
 * Converts valid json into an instance of std::optional<T> or a std::nullopt if
 * the json is not a valid representation of an instance of type T.
 */
template <typename T> requires TypeSupportedByEasySerDes<T>
std::optional<T> Deserialise(const nlohmann::json& serialised);

/**
 * Converts valid json into an instance of type T or if it is invalid,
 * terminates execution of the program.
 */
template <typename T> requires TypeSupportedByEasySerDes<T>
T DeserialiseWithoutChecks(const nlohmann::json& serialised);


template <JsonSerialserConcept T>
class JsonSerialiser<T> {
public:
    static bool Validate(const nlohmann::json& serialised)
    {
        return T::Validate(serialised);
    }

    static nlohmann::json Serialise(const T& value)
    {
        return T::Serialise(value);
    }

    static T Deserialise(const nlohmann::json& serialised)
    {
        return T::Deserialise(serialised);
    }
};

/**
 * Some JsonSerialiser<T> types may need to track information between distinct
 * user calls to Serialise or Deserialise (e.g. shared_ptr ideally should return
 * the same shared_ptr for repeat calls to Deserialise of the same serialised
 * shared_ptr).
 *
 * It is important to allow the user to decide when to clear these chaches, so
 * any type wishing to cache data must register a way to clear the cache here.
 */
class CacheManager {
public:
    static void AddEndOfOperationCallback(std::function<void()>&& cacheClearingcallback)
    {
        cacheClearingCallbacks.push_back(std::move(cacheClearingcallback));
    }

    static void ClearCaches()
    {
        for (auto& callback : cacheClearingCallbacks) {
            std::invoke(callback);
        }
    }

private:
    static inline std::vector<std::function<void()>> cacheClearingCallbacks{};
};

///
/// Implementation of API functions
///

template <typename T> requires TypeSupportedByEasySerDes<T>
bool Validate(const nlohmann::json& serialised)
{
    return JsonSerialiser<T>::Validate(serialised);
}

template <typename T> requires TypeSupportedByEasySerDes<T>
nlohmann::json Serialise(const T& value)
{
    return JsonSerialiser<T>::Serialise(value);
}

template <typename T> requires TypeSupportedByEasySerDes<T>
std::optional<T> Deserialise(const nlohmann::json& serialised)
{
    if (JsonSerialiser<T>::Validate(serialised)) {
        return std::make_optional(JsonSerialiser<T>::Deserialise(serialised));
    } else {
        return std::nullopt;
    }
}

template <typename T> requires TypeSupportedByEasySerDes<T>
T DeserialiseWithoutChecks(const nlohmann::json& serialised)
{
    return JsonSerialiser<T>::Deserialise(serialised);
}

} // end namespace esd

#endif // EASYSERDESCORE_H
