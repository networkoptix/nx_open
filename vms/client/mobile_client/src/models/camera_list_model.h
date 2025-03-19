// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QSortFilterProxyModel>

#include <common/common_globals.h>
#include <nx/utils/impl_ptr.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/types/resource_types.h>
#include <nx/vms/client/core/context_from_qml_handler.h>
#include <nx/vms/client/mobile/window_context_aware.h>


Q_MOC_INCLUDE("core/resource/layout_resource.h")

class QnResource;
class QnLayoutResource;
class QnCameraListModelPrivate;

class QnCameraListModel: public QSortFilterProxyModel,
    public nx::vms::client::mobile::WindowContextAware,
    public nx::vms::client::core::ContextFromQmlHandler
{
    Q_OBJECT
    using base_type = QSortFilterProxyModel;

    Q_PROPERTY(QVariant filterIds READ filterIds WRITE setFilterIds
        NOTIFY filterIdsChanged)
    Q_PROPERTY(QnLayoutResource* layout
        READ rawLayout
        WRITE setRawLayout
        NOTIFY layoutChanged)    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(UuidList selectedIds
        READ selectedIds
        WRITE setSelectedIds
        NOTIFY selectedIdsChagned)


public:
    QnCameraListModel(QObject* parent = nullptr);
    virtual ~QnCameraListModel() override;

    virtual QHash<int, QByteArray> roleNames() const override;
    virtual QVariant data(const QModelIndex& index, int role) const override;

    QnLayoutResource* rawLayout() const;
    void setRawLayout(QnLayoutResource* value);

    QVariant filterIds() const;
    void setFilterIds(const QVariant& ids);

    UuidList selectedIds() const;
    void setSelectedIds(const UuidList& value);

    int count() const;

    Q_INVOKABLE void setSelected(int row, bool selected);
    Q_INVOKABLE int rowByResource(QnResource* resource) const;
    Q_INVOKABLE nx::Uuid resourceIdByRow(int row) const;

    Q_INVOKABLE QnResource* nextResource(QnResource* resource) const;
    Q_INVOKABLE QnResource* previousResource(QnResource* resource) const;

public slots:
    void refreshThumbnail(int row);
    void refreshThumbnails(int from, int to);

signals:
    void layoutChanged();
    void countChanged();
    void filterIdsChanged();
    void selectedIdsChagned();

protected:
    virtual bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;
    virtual bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private:
    virtual void onContextReady() override;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};
