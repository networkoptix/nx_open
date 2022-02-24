// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "model_row_iterator.h"

#include <nx/utils/log/assert.h>

namespace nx::utils {

ModelRowIterator ModelRowIterator::cbegin(int role, const QAbstractItemModel* model,
    int column, const QModelIndex& parent)
{
    NX_ASSERT(model);
    return ModelRowIterator(role, model, 0, column, parent);
}

ModelRowIterator ModelRowIterator::cend(int role, const QAbstractItemModel* model,
    int column, const QModelIndex& parent)
{
    const int row = NX_ASSERT(model) ? model->rowCount(parent) : 0;
    return ModelRowIterator(role, model, row, column, parent);
}

ModelRowIterator::ModelRowIterator(int role,
    const QAbstractItemModel* model,
    int row,
    int column,
    const QModelIndex& parent)
    :
    m_role(role),
    m_model(model),
    m_parent(parent),
    m_column(column),
    m_row(row)
{
}

ModelRowIterator::difference_type ModelRowIterator::operator-(const ModelRowIterator& other) const
{
    return NX_ASSERT(isSibling(other))
        ? m_row - other.m_row
        : 0;
}

ModelRowIterator ModelRowIterator::operator++(int)
{
    ModelRowIterator result(*this);
    m_row++;
    return result;
}

ModelRowIterator ModelRowIterator::operator--(int)
{
    ModelRowIterator result(*this);
    m_row--;
    return result;
}

bool ModelRowIterator::operator==(const ModelRowIterator& other) const
{
    return m_row == other.m_row && NX_ASSERT(isSibling(other));
}

QModelIndex ModelRowIterator::index() const
{
    return NX_ASSERT(m_model)
        ? m_model->index(m_row, m_column, m_parent)
        : QModelIndex();
}

ModelRowIterator ModelRowIterator::sibling(int row) const
{
    return ModelRowIterator(m_role, m_model, row, m_column, m_parent);
}

bool ModelRowIterator::isSibling(const ModelRowIterator& other) const
{
    return m_role == other.m_role
        && m_model == other.m_model
        && m_column == other.m_column
        && m_parent == other.m_parent;
}

} // namespace nx:utils
