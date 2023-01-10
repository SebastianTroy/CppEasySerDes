#ifndef CONTEXT_H
#define CONTEXT_H

#include <map>
#include <memory>
#include <any>
#include <vector>

namespace esd {

namespace detail {

/**
 * Returns a nicer and more consistent name than typeid(T).name().
 *
 * Consistency is vital for saving on one system and loading on another.
 */
template <typename T>
[[ nodiscard ]] constexpr std::string_view TypeName()
{
    std::string_view name, prefix, suffix;
#ifdef __clang__
    name = __PRETTY_FUNCTION__;
    prefix = "std::string_view (anonymous namespace)::TypeName() [T = ";
    suffix = "]";
#elif defined(__GNUC__)
    name = __PRETTY_FUNCTION__;
    prefix = "constexpr std::string_view {anonymous}::TypeName() [with T = ";
    suffix = "; std::string_view = std::basic_string_view<char>]";
#elif defined(_MSC_VER)
    name = __FUNCSIG__;
    prefix = "class std::basic_string_view<char,struct std::char_traits<char> > __cdecl `anonymous-namespace'::TypeName<";
    suffix = ">(void)";
#endif
    name.remove_prefix(prefix.size());
    name.remove_suffix(suffix.size());
    return name;
}

template <typename T>
[[ nodiscard ]] std::string TypeNameStr()
{
    return std::string(TypeName<T>());
}

} // end namespace detail

/**
 * Stores information that needs to persist between
 * `Serialiser<T>::Validate/Serialise/Deserialise calls`, but needs to be
 * released between `esd::Validate/Serialise/Deserialise` calls.
 */
class Context {
public:
    template <typename CacheType>
    requires std::is_default_constructible_v<CacheType>
    CacheType& GetCache(const std::string& cacheName)
    {
        std::string cacheKey = cacheName + "::" + detail::TypeNameStr<CacheType>();
        std::unique_ptr<std::any>& cachePointer = caches_[cacheKey];
        if (cachePointer == nullptr) {
            cachePointer = std::make_unique<std::any>();
            // Create empty cache
            *cachePointer = CacheType{};
        }
        return std::any_cast<CacheType&>(*cachePointer);
    }

    void LogError(std::string&& error)
    {
        errors_.push_back(std::move(error));
    }

private:
    std::map<std::string, std::unique_ptr<std::any>> caches_;
    std::vector<std::string> errors_;
};

} // end namespace esd

#endif // CONTEXT_H
