// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QAbstractProxyModel>

Q_MOC_INCLUDE("nx/vms/client/desktop/analytics/attribute_display_manager.h")

namespace nx::vms::client::desktop::analytics::taxonomy {

class AttributeDisplayManager;

class NX_VMS_CLIENT_DESKTOP_API AnalyticsDialogTableModel: public QAbstractProxyModel
{
    Q_OBJECT
    using base_type = QAbstractProxyModel;

    Q_PROPERTY(AttributeDisplayManager* attributeManager
        READ attributeManager WRITE setAttributeManager)
    Q_PROPERTY(QStringList objectTypeIds READ objectTypeIds WRITE setObjectTypeIds)

public:
    explicit AnalyticsDialogTableModel(QObject* parent = nullptr);

    virtual QModelIndex parent(const QModelIndex& child) const override;
    virtual QModelIndex mapToSource(const QModelIndex& proxyIndex) const override;
    virtual QModelIndex mapFromSource(const QModelIndex& sourceIndex) const override;
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    virtual QVariant data(const QModelIndex& index, int role) const override;
    virtual int rowCount(const QModelIndex& parent) const override;
    virtual int columnCount(const QModelIndex& parent) const override;
    virtual QHash<int, QByteArray> roleNames() const override;
    virtual QModelIndex index(int row, int column, const QModelIndex& parent = {}) const override;
    virtual void setSourceModel(QAbstractItemModel* model) override;

    AttributeDisplayManager* attributeManager() const;
    void setAttributeManager(AttributeDisplayManager* manager);

    QStringList objectTypeIds() const;
    void setObjectTypeIds(const QStringList& objectTypeIds);

    static void registerQmlType();

private slots:
    void reloadAttributes();

private:
    QStringList m_columnNames;
    AttributeDisplayManager* m_attributeManager = nullptr;
    QStringList m_objectTypeIds;
};

} // nx::vms::client::desktop
