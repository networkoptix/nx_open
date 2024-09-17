// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>
#include <string>
#include <vector>

namespace nx::reflect {

/**
 * The result of an attempt to deserialize a type from a string (e.g., JSON or urlencoded).
 * Contains a flag indicating success or failure, an error description, and a fragment of the input
 * where the error occurred so that it is easier to find an issue in the data.
 */
struct NX_REFLECT_API DeserializationResult
{
    bool success = true;
    std::string errorDescription;

    /**
     * A fragment of an input string which wasn't parsed
     */
    std::string firstBadFragment;

    /**
     * A name of the least common ancestor of object type of the field which wasn't deserialized,
     * if exists.
     */
    std::optional<std::string> firstNonDeserializedField;

    struct Field
    {
        std::string name;
        std::vector<Field> fields;

        bool operator==(const Field&) const = default;
    };

    using Fields = std::vector<Field>;
    Fields fields;

    DeserializationResult(bool result);
    DeserializationResult() = default;
    DeserializationResult(
        bool result,
        std::string errorDescription,
        std::string badFragment,
        std::optional<std::string> notDeserializedField = std::nullopt);

    operator bool() const noexcept;

    std::string toString() const;

    // TODO: Replace with `fields.emplace_back(std::move(name), std::move(fields_))` when newer
    // clang will be used.
    void addField(std::string name, std::vector<Field> fields_)
    {
        Field f;
        f.name = std::move(name);
        f.fields = std::move(fields_);
        fields.push_back(std::move(f));
    }

    template<typename Member>
    void addField(const Member& member, std::vector<Field> fields_)
    {
        addField(std::string{member.name()}, std::move(fields_));
    }
};

} // namespace nx::reflect
