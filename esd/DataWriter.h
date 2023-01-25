#ifndef ESD_DATAWRITER_H
#define ESD_DATAWRITER_H

#include "Context.h"

#include <nlohmann/json.hpp>

#include <string>
#include <sstream>

namespace esd {

// FIXME would rather include Core.h than redeclare this here, need to remove esd namespace level functions from core first
template <typename T>
class Serialiser;

class DataWriter {
public:
    DataWriter() = delete;
    DataWriter(const DataWriter& other) = delete;
    DataWriter(DataWriter&& other) = delete;
    DataWriter& operator=(const DataWriter& other) = delete;
    DataWriter& operator=(DataWriter&& other) = delete;

    static DataWriter Create(Context& context, std::string&& name, nlohmann::json& dataStore)
    {
        return DataWriter(context, std::move(name), dataStore, nullptr);
    }

    ~DataWriter()
    {
        switch (format_) {
        case Format::Unset :
            LogError("~DataWriter destructed without being used (Note: If this is intentional, set format to " + ToString(Format::Object) + " to prevent this error).");
            break;
        case Format::Value :
            if (remainingWrites_ > 0) {
                LogError("~DataWriter destructed before value was written.");
            }
            break;
        case Format::Array :
            if (remainingWrites_ > 0) {
                LogError("~DataWriter destructed with " + std::to_string(remainingWrites_) + " values still to PushBack.");
            }
            break;
        case Format::Object :
            break;
        }
    }

    void SetFormatToValue()
    {
        if (format_ != Format::Unset) {
            LogError("DataWriter::SetFormatToValue Cannot change DataWriter's Format to " + ToString(Format::Value) + ", already set to " + ToString(format_) + ".");
            return;
        }
        format_ = Format::Value;
        remainingWrites_ = 1;
        capacity_ = 1;
    }

    void SetFormatToArray(size_t count)
    {
        if (format_ != Format::Unset) {
            LogError("DataWriter::SetFormatToArray Cannot change DataWriter's Format to " + ToString(Format::Array) + ", already set to " + ToString(format_) + ".");
            return;
        }
        format_ = Format::Array;
        dataStore_ = nlohmann::json::array();
        remainingWrites_ = count;
        capacity_ = count;
    }

    void SetFormatToObject()
    {
        if (format_ != Format::Unset) {
            LogError("DataWriter::SetFormatToObject Cannot change DataWriter's Format to " + ToString(Format::Object) + ", already set to " + ToString(format_) + ".");
            return;
        }
        format_ = Format::Object;
        dataStore_ = nlohmann::json::object();
        // ignore expectedWrites, object could be empty
    }

    template <typename T>
    void Write(const T& value)
    {
        if (format_ != Format::Value) {
            LogError("DataWriter::Write Must set Format to " + ToString(Format::Value) + " before writing a value.");
            return;
        }

        if (remainingWrites_ == 0) {
            LogError("DataWriter::Write Repeat call detected, to store multiple values, set Format to " + ToString(Format::Array) + " or " + ToString(Format::Object) + ".");
            return;
        }

        constexpr bool requiresDelegation = !std::is_arithmetic_v<T> && !std::same_as<std::string, T> && !std::same_as<T, nlohmann::json>;
        if constexpr (requiresDelegation) {
            Serialiser<T>::Serialise(DataWriter(context_, detail::TypeNameStr<T>() + ":delegate", dataStore_, this), value);
        } else if (!requiresDelegation) {
            dataStore_ = value;
        }

        --remainingWrites_;
    }

    // TODO consider adding two "WriteList" overloads
    // A "requires std::range/span" version that only accepts homogenous data, but can calculate the size
    // A param pack version that accepts heterogenious data, and therefore knows the count at compile-time
    // These can delegate to the SetCapacity() and PushBack() methods, which can be left available to the user
    // This would free up the need for the user to specify the format from within Serialiser<T> (i.e. auto deduce the format and remove the ability to set it manually from the user)
    // Forseeable issues: span/range does not guarantee there will be a size() function available? (percaps there is a std way to get the count in a span/range? see std::ranges::sized_range, a non-sized range could be iterated first to count, then iterated to write

    template <typename T>
    void PushBack(const T& value)
    {
        if (format_ != Format::Array) {
            LogError("DataWriter::PushBack Must set Format to " + ToString(Format::Array) + " before writing values.");
            return;
        }

        if (remainingWrites_ == 0) {
            LogError("DataWriter::PushBack Cannot call PushBack again, capacity already reached (" + std::to_string(capacity_) + ").");
            return;
        }

        constexpr bool requiresDelegation = !std::is_arithmetic_v<T> && !std::same_as<std::string, T> && !std::same_as<T, nlohmann::json>;
        if constexpr (requiresDelegation) {
            nlohmann::json itemData;
            Serialiser<T>::Serialise(DataWriter(context_, detail::TypeNameStr<T>() + ":item[" + std::to_string(capacity_ - remainingWrites_) + "]", itemData, this), value);
            dataStore_.push_back(itemData);
        } else if (!requiresDelegation) {
            dataStore_.push_back(value);
        }

        --remainingWrites_;
    }

    template <typename T>
    void Insert(const std::string& label, const T& value)
    {
        if (format_ != Format::Object) {
            LogError("DataWriter::Insert Must set Format to " + ToString(Format::Object) + " before inseting a value.");
            return;
        }

        if (label.empty()) {
            LogError("DataWriter::Insert Cannot use empty label.");
            return;
        }

        if (dataStore_.contains(label)) {
            LogError("DataWriter::Insert Cannot re-use label " + label + ".");
            return;
        }

        constexpr bool requiresDelegation = !std::is_arithmetic_v<T> && !std::same_as<std::string, T> && !std::same_as<T, nlohmann::json>;
        if constexpr (requiresDelegation) {
            nlohmann::json valueData;
            Serialiser<T>::Serialise(DataWriter(context_, detail::TypeNameStr<T>() + ":" + label, valueData, this), value);
            dataStore_[label] = valueData;
        } else if (!requiresDelegation) {
            dataStore_[label] = value;
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
    enum class Format {
        Unset,
        Value,
        Array,
        Object,
    };

    DataWriter* parent_;
    std::string stackFrameName_;

    Context& context_;
    Format format_;
    std::size_t capacity_;
    std::size_t remainingWrites_;

    // TODO move dependance on this
    nlohmann::json& dataStore_;

    DataWriter(Context& context, std::string&& name, nlohmann::json& dataStore, DataWriter* parent)
        : parent_(parent)
        , stackFrameName_(name)
        , context_(context)
        , format_(Format::Unset)
        , capacity_(0)
        , remainingWrites_(0)
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

    void CollectPath(std::ostream& pathBuilder, bool hasChild)
    {
        if (parent_) {
            parent_->CollectPath(pathBuilder, true);
        }
        pathBuilder << stackFrameName_ << (hasChild ? "/" : ":");
    }

};

} // end namespace esd

#endif // ESD_DATAWRITER_H
