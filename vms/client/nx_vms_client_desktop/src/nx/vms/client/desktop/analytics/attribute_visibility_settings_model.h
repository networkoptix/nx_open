// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QAbstractListModel>

Q_MOC_INCLUDE("nx/vms/client/desktop/analytics/attribute_display_manager.h")

namespace nx::vms::client::desktop::analytics::taxonomy {

class AttributeDisplayManager;

class AttributeVisibilitySettingsModel: public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(nx::vms::client::desktop::analytics::taxonomy::AttributeDisplayManager*
        attributeManager READ attributeManager WRITE setAttributeManager
        NOTIFY attributeManagerChanged)
    Q_PROPERTY(QStringList objectTypeIds READ objectTypeIds WRITE setObjectTypeIds
        NOTIFY objectTypeIdsChanged)

public:
    static void registerQmlType();

    AttributeVisibilitySettingsModel();

    AttributeDisplayManager* attributeManager() const;
    void setAttributeManager(AttributeDisplayManager* attributeManager);

    QStringList objectTypeIds() const;
    void setObjectTypeIds(const QStringList& objectTypeIds);

    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual QVariant data(const QModelIndex& index, int role) const override;
    virtual bool setData(const QModelIndex& index, const QVariant& value, int role) override;
    virtual Qt::ItemFlags flags(const QModelIndex& index) const override;
    virtual bool dropMimeData(
        const QMimeData* data,
        Qt::DropAction action,
        int row,
        int column,
        const QModelIndex& parent) override;
    virtual QHash<int, QByteArray> roleNames() const override;

signals:
    void attributeManagerChanged();
    void objectTypeIdsChanged();

private:
    void reloadAttributes();

private:
    AttributeDisplayManager* m_attributeManager = nullptr;

    QStringList m_objectTypeIds;

    QStringList m_attributes;

    bool m_movingRows = false;
};

} // namespace nx::vms::client::desktop::analytics::taxonomy
