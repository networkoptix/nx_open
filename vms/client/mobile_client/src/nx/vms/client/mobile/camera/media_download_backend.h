// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/core/context_from_qml_handler.h>
#include <nx/vms/client/core/resource/resource_fwd.h>
#include <nx/vms/client/mobile/window_context_aware.h>

Q_MOC_INCLUDE("core/resource/resource.h")

namespace nx::vms::client::mobile {

/**
 * Class generates the media download link and requests OS to open it in the default browser.
 * Functionality is available for the cloud connections and systems 6.0 and higher.
 */
class MediaDownloadBackend:
    public QObject,
    public WindowContextAware,
    public core::ContextFromQmlHandler
{
    Q_OBJECT

    using base_type = WindowContextAware;

    Q_PROPERTY(bool isDownloadAvailable
        READ isDownloadAvailable
        NOTIFY downloadAvailabilityChanged)

    Q_PROPERTY(QnResource* resource
        READ rawResource
        WRITE setRawResource
        NOTIFY resourceChanged)

public:
    static void registerQmlType();

    MediaDownloadBackend(QObject* parent = nullptr);
    virtual ~MediaDownloadBackend() override;

    bool isDownloadAvailable() const;

    Q_INVOKABLE void downloadVideo(qint64 startTimeMs,
        qint64 durationMs);

    QnResource* rawResource() const;
    void setRawResource(QnResource* value);

signals:
    void downloadAvailabilityChanged();
    void resourceChanged();
    void errorOccured(const QString& title, const QString& description);

private:
    void onContextReady();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::mobile
