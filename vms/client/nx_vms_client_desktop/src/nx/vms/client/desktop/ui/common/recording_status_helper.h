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

enum class RecordingStatus
{
    recordingContinious,
    recordingMetadataOnly,
    recordingMetadataAndLQ,
    recordingScheduled,
    noRecordingOnlyArchive,
    noRecordingNoArchive
};

class NX_VMS_CLIENT_DESKTOP_API RecordingStatusHelper: public QObject
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
        RecordingStatus recordingStatus,
        nx::vms::api::RecordingMetadataTypes metadataTypes);

    static QString shortTooltip(
        RecordingStatus recordingStatus,
        nx::vms::api::RecordingMetadataTypes metadataTypes);
    static QString shortTooltip(const QnVirtualCameraResourcePtr& camera);

    static QString qmlIconName(
        RecordingStatus recordingStatus,
        nx::vms::api::RecordingMetadataTypes metadataTypes);

    static QIcon icon(
        RecordingStatus recordingStatus,
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

private:
    nx::vms::api::RecordingMetadataTypes m_metadataTypes = nx::vms::api::RecordingMetadataType::none;
    RecordingStatus m_recordingStatus = RecordingStatus::noRecordingNoArchive;

    QnVirtualCameraResourcePtr m_camera;
    nx::utils::ScopedConnections m_connections;
};

} // namespace nx::vms::client::desktop
