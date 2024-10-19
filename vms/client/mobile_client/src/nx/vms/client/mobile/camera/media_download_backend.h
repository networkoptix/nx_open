// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/mobile/system_context_aware.h>
#include <nx/utils/impl_ptr.h>

namespace nx::vms::client::mobile {

/**
 * Class generates the media download link and requests OS to open it in the default browser.
 * Functionality is available for the cloud connections and systems 6.0 and higher.
 */
class MediaDownloadBackend: public QObject, public SystemContextAware
{
    Q_OBJECT

    using base_type = SystemContextAware;

    Q_PROPERTY(bool isDownloadAvailable
        READ isDownloadAvailable
        NOTIFY downloadAvailabilityChanged)

    Q_PROPERTY(nx::Uuid cameraId
        READ cameraId
        WRITE setCameraId
        NOTIFY cameraIdChanged)

public:
    static void registerQmlType();

    MediaDownloadBackend(QObject* parent = nullptr);
    virtual ~MediaDownloadBackend() override;

    bool isDownloadAvailable() const;

    Q_INVOKABLE void downloadVideo(qint64 startTimeMs,
        qint64 durationMs);

    nx::Uuid cameraId() const;
    void setCameraId(const nx::Uuid& value);

signals:
    void downloadAvailabilityChanged();
    void cameraIdChanged();
    void errorOccured(const QString& title, const QString& description);

private:
    void showDownloadProcessError();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::mobile
