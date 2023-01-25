#ifndef DATAREADER_H
#define DATAREADER_H

#include "Context.h"

#include <nlohmann/json.hpp>

#include <string>
#include <sstream>
#include <optional>

namespace esd {

// FIXME would rather include Core.h than redeclare this here, need to remove esd namespace level functions from core first
template <typename T>
class Serialiser;

class DataReader {
public:
    DataReader() = delete;
    DataReader(const DataReader& other) = delete;
    DataReader(DataReader&& other) = delete;
    DataReader& operator=(const DataReader& other) = delete;
    DataReader& operator=(DataReader&& other) = delete;

    static DataReader Create(Context& context, std::string&& name, const nlohmann::json& dataStore)
    {
        return DataReader(context, std::move(name), dataStore, nullptr);
    }

    template <typename T>
    std::optional<T> Read()
    {
        SetFormatIfUnset(Format::Value);

        if (format_ != Format::Value) {
            LogError("DataReader::ReadOr Cannot access data as value, format has already been set to " + ToString(format_) + " by a previous access.");
            return std::nullopt;
        }

        if (remainingReads_ == 0) {
            LogError("DataReader::ReadOr Repeat call detected, to read multiple values, data format must be " + ToString(Format::Array) + " or " + ToString(Format::Object) + ".");
            return std::nullopt;
        }

        if (dataStore_.is_array()) {
            LogError("DataReader::ReadOr Cannot access data as a value, underlying data structure contains an array.");
            return std::nullopt;
        } else if (dataStore_.is_object()) {
            LogError("DataReader::ReadOr Cannot access data as a value, underlying data structure contains an object.");
            return std::nullopt;
        }

        constexpr bool requiresDelegation = !std::is_arithmetic_v<T> && !std::same_as<std::string, T> && !std::same_as<T, nlohmann::json>;
        if constexpr (requiresDelegation) {
            return Serialiser<T>::Deserialise(DataReader(context_, detail::TypeNameStr<T>() + ":delegate", dataStore_, this));
        } else if (!requiresDelegation) {
            return dataStore_;
        }

        --remainingReads_;
    }

    std::optional<std::size_t> Size()
    {
        SetFormatIfUnset(Format::Array);

        if (format_ != Format::Array) {
            LogError("DataReader::Size Cannot access data as an array, format has already been set to " + ToString(format_) + " by a previous access.");
            return std::nullopt;
        }

        if (dataStore_.is_object()) {
            LogError("DataReader::Size Cannot access data as an array, underlying data structure contains an object.");
            return std::nullopt;
        } else if (!dataStore_.is_array()) {
            LogError("DataReader::Size Cannot access data as an array, underlying data structure contains a value.");
            return std::nullopt;
        }

        capacity_ = dataStore_.size();

        return capacity_;
    }

    template <typename T>
    std::optional<T> PopBack()
    {
        if (format_ == Format::Unset) {
            LogError("DataReader::PopBackOr Cannot access array data before reading the DataReader.Size of the array.");
            return std::nullopt;
        }

        if (format_ != Format::Array) {
            LogError("DataReader::PopBackOr Cannot access data as array, format has already been set to " + ToString(format_) + " by a previous access.");
            return std::nullopt;
        }

        if (remainingReads_ == 0) {
            LogError("DataReader::PopBackOr Cannot call PopBack again, all values have been read (" + std::to_string(capacity_) + ").");
            return std::nullopt;
        }

        constexpr bool requiresDelegation = !std::is_arithmetic_v<T> && !std::same_as<std::string, T> && !std::same_as<T, nlohmann::json>;
        if constexpr (requiresDelegation) {
            return Serialiser<T>::Deserialise(DataReader(context_, detail::TypeNameStr<T>() + ":delegate", dataStore_, this));
        } else if (!requiresDelegation) {
            return dataStore_[capacity_ - remainingReads_];
        }

        --remainingReads_;
    }

    template <typename T>
    requires std::ranges::range<T> && requires (T t) { T::value_type; t.insert(t.end(), declval(T::value_type)); }
    std::optional<T> ReadRange()
    {
        T items;
        std::size_t successfulReads = 0;
        const std::optional<std::size_t> count = Size();
        if (count) {
            if constexpr (requires (T t, std::size_t c){ { t.reserve(c) } -> std::same_as<void>; }) {
                items.reserve(*count);
            }

            while (successfulReads < *count) {
                if (auto item = PopBack<typename T::value_type>()) {
                    items.insert(items.end(), std::move(*item));
                    ++successfulReads;
                } else {
                    break;
                }
            }

            if (successfulReads == count) {
                return items;
            }
        }

        LogError("DataReader::ReadRange Failed to read item at index " + std::to_string(successfulReads) + ", while building a range of type " + detail::TypeNameStr<T>() + ".");
        return std::nullopt;
    }

    template <typename T>
    std::optional<T> Extract(const std::string& label)
    {
        SetFormatIfUnset(Format::Object);

        if (format_ != Format::Object) {
            LogError("DataReader::ExtractOr Cannot access data as object, format has already been set to " + ToString(format_) + " by a previous access.");
            return std::nullopt;
        }

        // FIXME use nlohmann::ordered_json and enshrine order guarantee so that alternative underlying data types don't need to support named value lookup
        if (!dataStore_.contains(label)) {
            LogError("DataReader::ExtractOr Missing sub-value with label '" + label +  "'.");
            return std::nullopt;
        }

        constexpr bool requiresDelegation = !std::is_arithmetic_v<T> && !std::same_as<std::string, T> && !std::same_as<T, nlohmann::json>;
        if constexpr (requiresDelegation) {
            return Serialiser<T>::Deserialise(DataReader(context_, detail::TypeNameStr<T>() + ":delegate", dataStore_[label], this));
        } else if (!requiresDelegation) {
            return dataStore_[label];
        }
    }

    void LogError(std::string&& error)
    {
        std::stringstream errorAndPath;
        CollectPath(errorAndPath, false);
        errorAndPath << " " << error;
        context_.LogError(errorAndPath.str());
    }

private:
    // FIXME much of the private section is duplicate of DataWriter, unify in base class called DataStack

    enum class Format {
        Unset,
        Value,
        Array,
        Object,
    };

    DataReader* parent_;
    std::string stackFrameName_;

    Context& context_;
    Format format_;
    std::size_t capacity_;
    std::size_t remainingReads_;

    // TODO move dependance on this
    const nlohmann::json& dataStore_;

    DataReader(Context& context, std::string&& name, const nlohmann::json& dataStore, DataReader* parent)
        : parent_(parent)
        , stackFrameName_(name)
        , context_(context)
        , format_(Format::Unset)
        , capacity_(0)
        , remainingReads_(0)
        , dataStore_(dataStore)
    {
    }

    static std::string ToString(Format type)
    {
        switch (type) {
        case Format::Unset :
            return "Unset";
        case Format::Value :
            return "Value";
        case Format::Array :
            return "Array";
        case Format::Object :
            return "Object";
        }
        return "";
    }

    void SetFormatIfUnset(Format format)
    {
        if (format_ == Format::Unset) {
            format_ = format;
        }
    }

    void CollectPath(std::ostream& pathBuilder, bool hasChild)
    {
        if (parent_) {
            parent_->CollectPath(pathBuilder, true);
        }
        pathBuilder << stackFrameName_ << (hasChild ? "/" : ":");
    }
};

} // end namespace esd

#endif // DATAREADER_H
