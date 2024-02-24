// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtGui/QIcon>

#include <core/resource/resource_fwd.h>
#include <nx/utils/scoped_connections.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/types/resource_types.h>

Q_MOC_INCLUDE("core/resource/resource.h")

namespace nx::vms::client::desktop {

class RecordingStatusHelper: public QObject
{
    Q_OBJECT
    Q_PROPERTY(QnResource* resource READ rawResource WRITE setRawResource NOTIFY resourceChanged)
    Q_PROPERTY(QString tooltip READ tooltip NOTIFY recordingModeChanged)
    Q_PROPERTY(QString shortTooltip READ shortTooltip NOTIFY recordingModeChanged)
    Q_PROPERTY(QString qmlIconName READ qmlIconName NOTIFY recordingModeChanged)

public:
    RecordingStatusHelper(QObject* parent = nullptr);

    QnVirtualCameraResourcePtr camera() const;
    void setCamera(const QnVirtualCameraResourcePtr& camera);

    QString tooltip() const;
    QString shortTooltip() const;
    QString qmlIconName() const;
    QIcon icon() const;

    static QString tooltip(
        nx::vms::api::RecordingType recordingType,
        nx::vms::api::RecordingMetadataTypes metadataTypes);

    static QString shortTooltip(
        nx::vms::api::RecordingType recordingType,
        nx::vms::api::RecordingMetadataTypes metadataTypes);
    static QString shortTooltip(const QnVirtualCameraResourcePtr& camera);

    static QString qmlIconName(
        nx::vms::api::RecordingType recordingType,
        nx::vms::api::RecordingMetadataTypes metadataTypes);

    static QIcon icon(
        nx::vms::api::RecordingType recordingType,
        nx::vms::api::RecordingMetadataTypes metadataTypes);
    static QIcon icon(const QnVirtualCameraResourcePtr& camera);

    static void registerQmlType();

signals:
    void resourceChanged();
    void recordingModeChanged();

private:
    void updateRecordingMode();

    QnResource* rawResource() const;
    void setRawResource(QnResource* value);

protected:
    virtual void timerEvent(QTimerEvent* event) override;

private:
    nx::vms::api::RecordingType m_recordingType = nx::vms::api::RecordingType::never;
    nx::vms::api::RecordingMetadataTypes m_metadataTypes = nx::vms::api::RecordingMetadataType::none;

    int m_updateTimerId = -1;
    QnVirtualCameraResourcePtr m_camera;
    nx::utils::ScopedConnections m_connections;
};

} // namespace nx::vms::client::desktop
