// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>

namespace nx::utils {

/**
 * A convenience helper to track current item model change operations.
 */
class NX_UTILS_API ModelOperation
{
public:
    enum Operation //< Intentionally scopeless.
    {
        none = 0,
        rowInsert = 0x01,
        rowRemove = 0x02,
        rowMove = rowInsert | rowRemove,
        columnInsert = 0x10,
        columnRemove = 0x20,
        columnMove = columnInsert | columnRemove,
        layoutChange = 0x40,
        modelReset = 0x80
    };

    ModelOperation() = default;
    ModelOperation(const ModelOperation&) = default;
    ModelOperation(Operation operation): m_operation(operation) {}

    operator int() const { return m_operation; }

    bool inProgress(Operation operation) const;

    bool canBegin(Operation operation) const;
    bool begin(Operation operation);
    bool end(Operation operation);

private:
    int m_operation = Operation::none;
};

QString toString(ModelOperation op);

} // namespace nx::utils
