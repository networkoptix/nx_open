// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QAbstractItemModel>

#include <client/client_globals.h>
#include <nx/vms/common/resource/camera_hotspots_data.h>

class QnResourcePool;

namespace nx::vms::client::desktop {

class CameraHotspotsItemModel: public QAbstractItemModel
{
    Q_OBJECT
    using base_type = QAbstractItemModel;

public:
    enum Column
    {
        IndexColumn,
        TargetColumn,
        ColorPaletteColumn,
        PointedCheckBoxColumn,
        DeleteButtonColumn,

        ColumnCount,
    };

    enum Role
    {
        HotspotCameraIdRole = Qn::ItemDataRoleCount + 1,
        HotspotColorRole,
    };

    CameraHotspotsItemModel(QnResourcePool* resourcePool, QObject* parent = nullptr);
    virtual ~CameraHotspotsItemModel() override;

    void setHotspots(const nx::vms::common::CameraHotspotDataList& hotspots);

    virtual int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    virtual QVariant headerData(int section, Qt::Orientation orientation,
        int role = Qt::DisplayRole) const override;

    virtual QModelIndex index(int row, int column,
        const QModelIndex& parent = QModelIndex()) const override;

    virtual QModelIndex parent(const QModelIndex& index) const override;
    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;

private:
    nx::vms::common::CameraHotspotDataList m_hotspots;
    QnResourcePool* m_resourcePool;
};

} // namespace nx::vms::client::desktop
