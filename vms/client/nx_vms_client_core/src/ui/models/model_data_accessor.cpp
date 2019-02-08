#include "model_data_accessor.h"

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

    connect(m_model, &QAbstractItemModel::modelReset,
        this, &ModelDataAccessor::countChanged);
    connect(m_model, &QAbstractItemModel::rowsRemoved,
        this, &ModelDataAccessor::countChanged);
    connect(m_model, &QAbstractItemModel::rowsInserted,
        this, &ModelDataAccessor::countChanged);
    connect(m_model, &QAbstractItemModel::rowsMoved,
        this, &ModelDataAccessor::rowsMoved);

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
    if (!m_model)
        return QVariant();

    if (!m_model->hasIndex(row, 0))
        return QVariant();

    const auto role = m_model->roleNames().key(roleName.toUtf8(), -1);
    if (role == -1)
        return QVariant();

    return m_model->data(m_model->index(row, 0), role);
}

} // namespace client
} // namespace nx
