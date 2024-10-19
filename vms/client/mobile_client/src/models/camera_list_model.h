// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QSortFilterProxyModel>

#include <common/common_globals.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/types/resource_types.h>
#include <nx/vms/client/mobile/current_system_context_aware.h>

class QnCameraListModelPrivate;

class QnCameraListModel:
    public QSortFilterProxyModel,
    public nx::vms::client::mobile::CurrentSystemContextAware
{
    Q_OBJECT

    Q_PROPERTY(QVariant filterIds READ filterIds WRITE setFilterIds
        NOTIFY filterIdsChanged)
    Q_PROPERTY(QString layoutId READ layoutId WRITE setLayoutId NOTIFY layoutIdChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(UuidList selectedIds
        READ selectedIds
        WRITE setSelectedIds
        NOTIFY selectedIdsChagned)

    using base_type = QSortFilterProxyModel;

public:
    QnCameraListModel(QObject* parent = nullptr);
    virtual ~QnCameraListModel() override;

    virtual QHash<int, QByteArray> roleNames() const override;
    virtual QVariant data(const QModelIndex& index, int role) const override;

    QString layoutId() const;
    void setLayoutId(const QString& layoutId);

    QVariant filterIds() const;
    void setFilterIds(const QVariant& ids);

    UuidList selectedIds() const;
    void setSelectedIds(const UuidList& value);

    int count() const;

    Q_INVOKABLE void setSelected(int row, bool selected);
    Q_INVOKABLE int rowByResourceId(const nx::Uuid& resourceId) const;
    Q_INVOKABLE nx::Uuid resourceIdByRow(int row) const;

    Q_INVOKABLE QnResource* nextResource(const QnResource* resource) const;
    Q_INVOKABLE QnResource* previousResource(const QnResource* resource) const;

public slots:
    void refreshThumbnail(int row);
    void refreshThumbnails(int from, int to);

signals:
    void layoutIdChanged();
    void countChanged();
    void filterIdsChanged();
    void selectedIdsChagned();

protected:
    virtual bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;
    virtual bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private:
    QScopedPointer<QnCameraListModelPrivate> d_ptr;
    Q_DECLARE_PRIVATE(QnCameraListModel)
};
