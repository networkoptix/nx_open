// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "attribute_visibility_settings_model.h"

#include <QtCore/QIODevice>
#include <QtCore/QMimeData>
#include <QtCore/QScopedValueRollback>
#include <QtQml/QtQml>

#include <nx/vms/client/core/analytics/analytics_filter_model.h>
#include <nx/vms/client/desktop/analytics/attribute_display_manager.h>

namespace nx::vms::client::desktop::analytics::taxonomy {

void AttributeVisibilitySettingsModel::registerQmlType()
{
    qmlRegisterType<taxonomy::AttributeVisibilitySettingsModel>(
        "nx.vms.client.desktop.analytics", 1, 0, "AttributeVisibilitySettingsModel");
}

AttributeVisibilitySettingsModel::AttributeVisibilitySettingsModel()
{
}

QStringList AttributeVisibilitySettingsModel::objectTypeIds() const
{
    return m_objectTypeIds;
}

void AttributeVisibilitySettingsModel::setObjectTypeIds(const QStringList& objectTypeIds)
{
    if (m_objectTypeIds == objectTypeIds)
        return;

    m_objectTypeIds = objectTypeIds;
    emit objectTypeIdsChanged();

    reloadAttributes();
}

int AttributeVisibilitySettingsModel::rowCount(const QModelIndex& /*parent*/) const
{
    return m_attributes.count();
}

QVariant AttributeVisibilitySettingsModel::data(const QModelIndex& index, int role) const
{
    if (!NX_ASSERT(checkIndex(index, CheckIndexOption::IndexIsValid)))
        return {};

    const QString attribute = m_attributes[index.row()];

    switch (role)
    {
        case Qt::DisplayRole:
            return m_attributeManager->displayName(attribute);

        case Qt::CheckStateRole:
            return m_attributeManager->isVisible(attribute);

        default:
            return {};
    }
}

bool AttributeVisibilitySettingsModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!NX_ASSERT(checkIndex(index, CheckIndexOption::IndexIsValid)))
        return false;

    const QString attribute = m_attributes[index.row()];

    if (role == Qt::CheckStateRole)
    {
        const bool currentlyChecked = m_attributeManager->isVisible(attribute);
        if (currentlyChecked != value.toBool())
        {
            m_attributeManager->setVisible(attribute, value.toBool());
            emit dataChanged(index, index, {role});
            return true;
        }
    }

    return false;
}

Qt::ItemFlags AttributeVisibilitySettingsModel::flags(const QModelIndex& index) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid))
        return {};

    const QString attribute = m_attributes[index.row()];

    Qt::ItemFlags flags = Qt::ItemIsEnabled | Qt::ItemNeverHasChildren;

    if (m_attributeManager->canBeMoved(attribute))
        flags |= Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled;

    if (m_attributeManager->canBeHidden(attribute))
        flags |= Qt::ItemIsUserCheckable;

    return flags;
}

bool AttributeVisibilitySettingsModel::dropMimeData(
    const QMimeData* data,
    Qt::DropAction action,
    int /*row*/,
    int /*column*/,
    const QModelIndex& destination)
{
    const QString kMimeType = mimeTypes().first();

    if (action != Qt::MoveAction || !data->hasFormat(kMimeType)
        || !destination.flags().testFlag(Qt::ItemIsDropEnabled))
    {
        return false;
    }

    const int rowTo = destination.row();

    QByteArray encodedData = data->data(kMimeType);
    if (encodedData.isEmpty())
        return false;

    int rowFrom = 0;
    QDataStream stream(&encodedData, QIODevice::ReadOnly);
    stream >> rowFrom;
    if (rowFrom == rowTo)
        return true;

    const int destinationRow = rowTo + (rowFrom < rowTo ? 1 : 0);

    const QString attribute = m_attributes[rowFrom];
    const QString destinationAttribute = m_attributes[rowTo];

    {
        QScopedValueRollback<bool> guard(m_movingRows, true);

        beginMoveRows({}, rowFrom, rowFrom, {}, destinationRow);
        m_attributes.move(rowFrom, rowTo);
        m_attributeManager->placeAttributeBefore(attribute, destinationAttribute);
        endMoveRows();
    }

    return true;
}

QHash<int, QByteArray> AttributeVisibilitySettingsModel::roleNames() const
{
    static QHash<int, QByteArray> roleNames;
    if (roleNames.isEmpty())
    {
        roleNames = QAbstractListModel::roleNames();
        roleNames.insert(Qt::CheckStateRole, "checked");
    }
    return roleNames;
}

AttributeDisplayManager* AttributeVisibilitySettingsModel::attributeManager() const
{
    return m_attributeManager;
}

void AttributeVisibilitySettingsModel::setAttributeManager(
    AttributeDisplayManager* attributeManager)
{
    if (m_attributeManager == attributeManager)
        return;

    if (m_attributeManager)
        disconnect(m_attributeManager);

    m_attributeManager = attributeManager;

    if (m_attributeManager)
    {
        connect(m_attributeManager, &AttributeDisplayManager::attributesChanged,
            this, &AttributeVisibilitySettingsModel::reloadAttributes);
    }

    emit attributeManagerChanged();

    reloadAttributes();
}

void AttributeVisibilitySettingsModel::reloadAttributes()
{
    if (m_movingRows)
        return;

    beginResetModel();

    m_attributes.clear();

    if (m_attributeManager)
    {
        const QStringList relevantAttributes =
            m_attributeManager->attributesForObjectType(m_objectTypeIds)
                + m_attributeManager->builtInAttributes();

        m_attributes = m_attributeManager->attributes();
        m_attributes.erase(
            std::remove_if(m_attributes.begin(), m_attributes.end(),
                [&](const QString& attribute) { return !relevantAttributes.contains(attribute); }),
            m_attributes.end());
    }

    endResetModel();
}

} // namespace nx::vms::client::desktop::analytics::taxonomy
