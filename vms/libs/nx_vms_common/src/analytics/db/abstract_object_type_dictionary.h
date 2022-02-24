// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <QtCore/QString>

namespace nx::analytics::db {

class AbstractObjectTypeDictionary
{
public:
    virtual ~AbstractObjectTypeDictionary() = default;

    /**
     * NOTE: This function is not expected to block.
     * Doing so may degrade analytics DB performance significantly.
     */
    virtual std::optional<QString> idToName(const QString& id) const = 0;
};

} // namespace nx::analytics::db
