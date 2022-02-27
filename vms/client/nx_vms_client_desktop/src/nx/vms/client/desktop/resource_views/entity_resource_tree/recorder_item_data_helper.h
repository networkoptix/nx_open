// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QHash>

#include <core/resource/resource_fwd.h>

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

class CameraResourceIndex;

class RecorderItemDataHelper: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    RecorderItemDataHelper(const CameraResourceIndex* cameraResourceIndex);

    QString groupedDeviceName(const QString& recorderGroupId) const;
    bool isMultisensorCamera(const QString& recorderGroupId) const;
    bool isRecorder(const QString& recorderGroupId) const;

signals:
    void groupedDeviceNameChanged(const QString& recorderGroupId);

private:
    void onCameraAdded(const QnResourcePtr& cameraResource);
    void onCameraRemoved(const QnResourcePtr& cameraResource);
    void onCameraGroupIdChanged(const QnResourcePtr& cameraResource,
        const QString& previousRecorderGroupId);
    void onCameraGroupNameChanged(const QnResourcePtr& cameraResource);

private:
    QHash<QString, QSet<QnVirtualCameraResourcePtr>> m_camerasByRecorderGroupId;
};

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
