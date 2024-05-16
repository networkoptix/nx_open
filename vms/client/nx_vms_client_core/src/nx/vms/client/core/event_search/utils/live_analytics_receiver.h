// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QList>
#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <nx/media/meta_data_packet.h>
#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/core/network/remote_connection_aware.h>

namespace nx::vms::client::core {

class LiveAnalyticsReceiver:
    public QObject,
    public nx::vms::client::core::RemoteConnectionAware
{
    Q_OBJECT

public:
    LiveAnalyticsReceiver(QObject* parent = nullptr);
    LiveAnalyticsReceiver(const QnVirtualCameraResourcePtr& camera, QObject* parent = nullptr);
    virtual ~LiveAnalyticsReceiver() override;

    QnVirtualCameraResourcePtr camera() const;
    void setCamera(const QnVirtualCameraResourcePtr& value);

    QList<QnAbstractCompressedMetadataPtr> takeData();

signals:
    void dataOverflow(QPrivateSignal); //< takeData must be called as soon as possible.

private:
    class Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::core
