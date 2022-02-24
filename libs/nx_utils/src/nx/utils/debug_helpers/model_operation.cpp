// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "model_operation.h"

#include <QtCore/QStringList>

#include <nx/utils/log/assert.h>

namespace nx::utils {

namespace {

constexpr int kModelModificationMask = ModelOperation::layoutChange | ModelOperation::modelReset;
constexpr int kRowModificationMask = ModelOperation::rowMove | kModelModificationMask;
constexpr int kColumnModificationMask = ModelOperation::columnMove | kModelModificationMask;

} // namespace

bool ModelOperation::inProgress(Operation operation) const
{
    return (m_operation & operation) == operation;
}

bool ModelOperation::canBegin(Operation operation) const
{
    if ((operation & kModelModificationMask) != 0)
        return m_operation == Operation::none;

    if ((operation & kRowModificationMask) != 0)
        return (m_operation & kRowModificationMask) == 0;

    if ((operation & kColumnModificationMask) != 0)
        return (m_operation & kColumnModificationMask) == 0;

    NX_ASSERT(false);
    return false;
}

bool ModelOperation::begin(Operation operation)
{
    const bool result = canBegin(operation);
    m_operation |= operation;
    return result;
}

bool ModelOperation::end(Operation operation)
{
    const bool result = inProgress(operation);
    m_operation &= ~operation;
    return result;
}

QString toString(ModelOperation op)
{
    if (op == ModelOperation::none)
        return "none";

    QStringList items;
    if (op.inProgress(ModelOperation::rowMove))
        items.push_back("rowMove");
    else if (op.inProgress(ModelOperation::rowInsert))
        items.push_back("rowInsert");
    else if (op.inProgress(ModelOperation::rowRemove))
        items.push_back("rowRemove");

    if (op.inProgress(ModelOperation::columnMove))
        items.push_back("columnMove");
    else if (op.inProgress(ModelOperation::columnInsert))
        items.push_back("columnInsert");
    else if (op.inProgress(ModelOperation::columnRemove))
        items.push_back("columnRemove");

    if (op.inProgress(ModelOperation::layoutChange))
        items.push_back("layoutChange");

    if (op.inProgress(ModelOperation::modelReset))
        items.push_back("modelReset");

    return items.join("|");
}

} // namespace nx::utils
