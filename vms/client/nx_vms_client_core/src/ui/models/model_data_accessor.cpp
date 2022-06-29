// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "model_data_accessor.h"

#include <nx/utils/log/assert.h>

namespace nx {
namespace client {

ModelDataAccessor::ModelDataAccessor(QObject* parent):
    base_type(parent)
{
}

ModelDataAccessor::~ModelDataAccessor()
{
}

QVariant ModelDataAccessor::model() const
{
    return QVariant::fromValue(m_model.data());
}

void ModelDataAccessor::setModel(const QVariant& modelVariant)
{
    const auto model = modelVariant.value<QAbstractItemModel*>();

    if (m_model == model)
        return;

    if (m_model)
        m_model->disconnect(this);

    m_model = model;

    emit modelChanged();

    if (!m_model)
        return;

    connect(m_model.data(), &QAbstractItemModel::modelReset,
        this, &ModelDataAccessor::countChanged);
    connect(m_model.data(), &QAbstractItemModel::rowsRemoved,
        this, &ModelDataAccessor::countChanged);
    connect(m_model.data(), &QAbstractItemModel::rowsInserted,
        this, &ModelDataAccessor::countChanged);
    connect(m_model.data(), &QAbstractItemModel::rowsMoved,
        this, &ModelDataAccessor::rowsMoved);

    QObject::connect(m_model, &QAbstractItemModel::rowsInserted, this,
        [this](const QModelIndex& /*parent*/, int first, int last)
        {
            emit rowsInserted(first, last);
        });
    QObject::connect(m_model, &QAbstractItemModel::rowsRemoved, this,
        [this](const QModelIndex& /*parent*/, int first, int last)
        {
            emit rowsRemoved(first, last);
        });

    connect(m_model, &QAbstractItemModel::dataChanged, this,
        [this](const QModelIndex& topLeft, const QModelIndex& bottomRight)
        {
            emit dataChanged(topLeft.row(), bottomRight.row());
        });

    emit countChanged();

    const auto rowCount = count();
    if (rowCount > 0)
        emit dataChanged(0, rowCount - 1);
}

int ModelDataAccessor::count() const
{
    return m_model ? m_model->rowCount() : 0;
}

QVariant ModelDataAccessor::getData(int row, const QString& roleName) const
{
    return m_model && m_model->hasIndex(row, 0)
        ? getData(m_model->index(row, 0), roleName)
        : QVariant();
}

QVariant ModelDataAccessor::getData(const QModelIndex& index, const QString& roleName) const
{
    if (!m_model || !index.isValid() || !NX_ASSERT(m_model->checkIndex(index)))
        return QVariant();

    const auto role = m_model->roleNames().key(roleName.toUtf8(), -1);
    if (role == -1)
        return QVariant();

    return m_model->data(index, role);
}

} // namespace client
} // namespace nx
