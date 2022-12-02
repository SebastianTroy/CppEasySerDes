#ifndef CURRENTCONTEXT_H
#define CURRENTCONTEXT_H

#include <map>
#include <memory>
#include <any>

namespace esd {

/**
 * Users can create an instance of this type in order to maintain context across
 * multiple calls to `esd::Validate`, `esd::Serialise`, `esd::Deserialise`, or
 * `esd::DeserialiseWithoutChecks`.
 */
class ContextStateLifetime;

namespace internal {

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

/**
 * Stores information that needs to persist between
 * `Serialiser<T>::Validate/Serialise/Deserialise calls`, but needs to be
 * released between `esd::Validate/Serialise/Deserialise` calls.
 *
 * Optionally the lifetime of this information can be extended by the user via
 * a ScopedContextReset object.
 *
 * TODO use this for error reporting
 * MAYBE use this to abstract the underlying storage format
 */
class CurrentContext {
public:
    template <typename CacheType>
    requires std::is_default_constructible_v<CacheType>
    static CacheType& GetCache(const std::string& cacheName)
    {
        std::string cacheKey = cacheName + "::" + std::string(internal::TypeName<CacheType>());
        std::unique_ptr<std::any>& cachePointer = caches_[cacheKey];
        if (cachePointer == nullptr) {
            cachePointer = std::make_unique<std::any>();
            // Create empty cache
            *cachePointer = CacheType{};
        }
        return std::any_cast<CacheType&>(*cachePointer);
    }

private:
    static inline std::map<std::string, std::unique_ptr<std::any>> caches_ {};

    friend ContextStateLifetime;
    static void Reset()
    {
        caches_.clear();
    }
};

} // end namespace internal

/**
 * Resets CurrentContext when the last existing ContextStateLifetime goes out of
 * scope.
 */
class ContextStateLifetime {
public:
    ContextStateLifetime()
    {
        ++count;
    }
    ContextStateLifetime(const ContextStateLifetime& other) = delete;
    ContextStateLifetime(ContextStateLifetime&& other) = delete;
    ContextStateLifetime& operator=(const ContextStateLifetime& other) = delete;
    ContextStateLifetime& operator=(ContextStateLifetime&& other) = delete;

    ~ContextStateLifetime()
    {
        if (--count == 0) {
            internal::CurrentContext::Reset();
        }
    }

private:
    static inline int count = 0;
};

} // end namespace esd

#endif // CURRENTCONTEXT_H
