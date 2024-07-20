// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_dialog_table_model.h"

#include <QtQml/QtQml>

#include <client/client_globals.h>
#include <nx/vms/client/core/client_core_globals.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/desktop/analytics/attribute_display_manager.h>
#include <nx/vms/client/desktop/event_search/models/analytics_search_list_model.h>
#include <nx/vms/client/desktop/ui/right_panel/models/right_panel_models_adapter.h>

namespace nx::vms::client::desktop::analytics::taxonomy {

AnalyticsDialogTableModel::AnalyticsDialogTableModel(QObject* parent):
    base_type(parent)
{
}

QModelIndex AnalyticsDialogTableModel::parent(const QModelIndex& /*child*/) const
{
    return {};
}

QModelIndex AnalyticsDialogTableModel::mapToSource(const QModelIndex& proxyIndex) const
{
    if (!proxyIndex.isValid())
        return {};

    NX_ASSERT(proxyIndex.model() == this);
    return createSourceIndex(proxyIndex.row(), proxyIndex.column(), proxyIndex.internalPointer());
}

QModelIndex AnalyticsDialogTableModel::mapFromSource(const QModelIndex& sourceIndex) const
{
    if (!sourceIndex.isValid())
        return {};

    NX_ASSERT(sourceIndex.model() == sourceModel());
    return createIndex(sourceIndex.row(), sourceIndex.column(), sourceIndex.internalPointer());
}

QVariant AnalyticsDialogTableModel::headerData(
    int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole
        || orientation != Qt::Horizontal
        || section >= m_columnNames.count())
    {
        return {};
    }

    return m_attributeManager->displayName(m_columnNames[section]);
}

QVariant AnalyticsDialogTableModel::data(const QModelIndex& index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid) || !sourceModel())
        return {};

    const auto columnName = m_columnNames[index.column()];
    const auto sourceIndex = mapToSource(this->index(index.row(), 0));
    switch (role)
    {
        case Qt::DisplayRole:
        {
            if (columnName == kDateTimeAttributeName)
                return sourceIndex.data(core::TimestampTextRole);

            if (columnName == kObjectTypeAttributeName)
                return sourceIndex.data(Qt::DisplayRole);

            if (columnName == kCameraAttributeName)
                return sourceIndex.data(core::DisplayedResourceListRole);

            if (columnName == kTitleAttributeName)
                return sourceIndex.data(core::ObjectTitleRole);

            const auto attributes = sourceIndex.data(core::AnalyticsAttributesRole)
                .value<core::analytics::AttributeList>();
            for (const auto& attribute: attributes)
            {
                if (attribute.id == columnName)
                    return AnalyticsSearchListModel::valuesText(attribute.displayedValues);
            }
            break;
        }

        case Qn::ResourceIconKeyRole:
        {
            if (columnName == kCameraAttributeName)
                return "text_buttons/camera_20.svg";

            if (columnName == kObjectTypeAttributeName)
                return sourceIndex.data(core::DecorationPathRole);

            break;
        }

        case Qt::ForegroundRole:
        {
            const auto attributes = sourceIndex.data(core::AnalyticsAttributesRole)
                .value<core::analytics::AttributeList>();
            for (const auto& attribute: attributes)
            {
                if (attribute.id == columnName)
                    return attribute.colorValues;
            }
            break;
        }
    }

    return {};
}

int AnalyticsDialogTableModel::rowCount(const QModelIndex& parent) const
{
    return sourceModel() ? sourceModel()->rowCount(parent) : 0;
}

int AnalyticsDialogTableModel::columnCount(const QModelIndex& /*parent*/) const
{
    return m_columnNames.count();
}

QHash<int, QByteArray> AnalyticsDialogTableModel::roleNames() const
{
    static QHash<int, QByteArray> roleNames;
    if (roleNames.isEmpty())
    {
        roleNames = base_type::roleNames();
        roleNames.insert({{Qn::ResourceIconKeyRole, "iconKey"}});
    }
    return roleNames;
}

QModelIndex AnalyticsDialogTableModel::index(int row, int column, const QModelIndex& parent) const
{
    return hasIndex(row, column, parent) ? createIndex(row, column) : QModelIndex();
}

void AnalyticsDialogTableModel::setSourceModel(QAbstractItemModel* model)
{
    if (model == sourceModel())
        return;

    beginResetModel();
    if (sourceModel())
        sourceModel()->disconnect(this);

    base_type::setSourceModel(model);
    if (model)
    {
        connect(model, &QAbstractItemModel::rowsAboutToBeInserted, this,
            [this](const QModelIndex& parent, int start, int end)
            {
                beginInsertRows(mapFromSource(parent), start, end);
            });

        connect(model, &QAbstractItemModel::rowsInserted, this,
            [this]()
            {
                endInsertRows();
            });

        connect(model, &QAbstractItemModel::rowsAboutToBeMoved, this,
            [](){ NX_ASSERT(false, "Source rows moving is not supported."); });

        connect(model, &QAbstractItemModel::rowsMoved, this,
            [](){ NX_ASSERT(false, "Source rows moving is not supported."); });

        connect(model, &QAbstractItemModel::rowsAboutToBeRemoved, this,
            [this](const QModelIndex& parent, int start, int end)
            {
                beginRemoveRows(mapFromSource(parent), start, end);
            });

        connect(model, &QAbstractItemModel::rowsRemoved, this,
            [this]()
            {
                endRemoveRows();
            });

        connect(model, &QAbstractItemModel::modelAboutToBeReset, this,
            [this](){ beginResetModel(); });
        connect(model, &QAbstractItemModel::modelReset, this,
            [this](){ endResetModel(); });
        connect(model, &QAbstractItemModel::dataChanged, this,
            [this](
                const QModelIndex& topLeft,
                const QModelIndex& bottomRight,
                const QList<int>& roles)
            {
                emit dataChanged(mapFromSource(topLeft), mapFromSource(bottomRight), roles);
            });

        connect(model, &QAbstractItemModel::layoutAboutToBeChanged, this,
            [](){ NX_ASSERT(false, "Source layout changing is not supported."); });

        connect(model, &QAbstractItemModel::layoutChanged, this,
            [](){ NX_ASSERT(false, "Source layout changing is not supported."); });
    }
    endResetModel();
}

AttributeDisplayManager* AnalyticsDialogTableModel::attributeManager() const
{
    return m_attributeManager;
}

void AnalyticsDialogTableModel::setAttributeManager(AttributeDisplayManager* manager)
{
    if (manager == m_attributeManager)
        return;

    if (m_attributeManager)
        m_attributeManager->disconnect(this);

    m_attributeManager = manager;

    if (m_attributeManager)
    {
        connect(m_attributeManager, &AttributeDisplayManager::attributesChanged,
            this, &AnalyticsDialogTableModel::reloadAttributes);
        connect(m_attributeManager, &AttributeDisplayManager::visibleAttributesChanged,
            this, &AnalyticsDialogTableModel::reloadAttributes);
    }
    reloadAttributes();
}

QStringList AnalyticsDialogTableModel::objectTypeIds() const
{
    return m_objectTypeIds;
}

void AnalyticsDialogTableModel::setObjectTypeIds(const QStringList& objectTypeIds)
{
    if (m_objectTypeIds == objectTypeIds)
        return;

    m_objectTypeIds = objectTypeIds;
    reloadAttributes();
}

void AnalyticsDialogTableModel::registerQmlType()
{
    qmlRegisterType<AnalyticsDialogTableModel>(
        "nx.vms.client.desktop.analytics", 1, 0, "AnalyticsDialogTableModel");
}

void AnalyticsDialogTableModel::reloadAttributes()
{
    beginResetModel();
    if (m_attributeManager)
    {
        const QStringList relevantColumns =
            m_attributeManager->attributesForObjectType(m_objectTypeIds)
                + m_attributeManager->builtInAttributes();
        m_columnNames = m_attributeManager->attributes();
        m_columnNames.erase(
            std::remove_if(m_columnNames.begin(), m_columnNames.end(),
                [&](const QString& attribute)
                {
                    return !relevantColumns.contains(attribute)
                        || !m_attributeManager->visibleAttributes().contains(attribute);
                }),
            m_columnNames.end());
    }
    else
    {
        m_columnNames.clear();
    }
    endResetModel();
    emit headerDataChanged(Qt::Horizontal, 0, m_columnNames.count() - 1);
}

} // namespace nx::vms::client::desktop
