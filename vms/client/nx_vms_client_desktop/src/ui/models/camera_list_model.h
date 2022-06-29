// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QAbstractItemModel>

#include <core/resource/resource_fwd.h>
#include <nx/utils/scoped_model_operations.h>
#include <ui/workbench/workbench_context_aware.h>

class QnCameraListModel:
    public ScopedModelOperations<QAbstractItemModel>,
    public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = ScopedModelOperations<QAbstractItemModel>;

public:
    enum Column
    {
        RecordingColumn,
        NameColumn,
        VendorColumn,
        ModelColumn,
        FirmwareColumn,
        IpColumn,
        MacColumn,
        LogicalIdColumn,
        ServerColumn,
        ColumnCount
    };

    explicit QnCameraListModel(QObject* parent = nullptr);
    virtual ~QnCameraListModel() override;

    virtual QModelIndex index(
        int row, int column, const QModelIndex& parent = QModelIndex()) const override;
    virtual QModelIndex parent(const QModelIndex& child) const override;
    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    virtual QVariant headerData(
        int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    QnMediaServerResourcePtr server() const;
    Q_SLOT void setServer(const QnMediaServerResourcePtr& server);

signals:
    void serverChanged();

private:
    void addCamera(const QnVirtualCameraResourcePtr& camera, bool emitSignals);
    void removeCamera(const QnVirtualCameraResourcePtr& camera);
    bool cameraFits(const QnVirtualCameraResourcePtr& camera) const;

    void handleResourceAdded(const QnResourcePtr& resource);
    void handleResourceRemoved(const QnResourcePtr& resource);
    void handleParentIdChanged(const QnResourcePtr& resource);
    void handleResourceChanged(const QnResourcePtr& resource);

private:
    QList<QnVirtualCameraResourcePtr> m_cameras;
    QnMediaServerResourcePtr m_server;
};
