// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/uuid.h>
#include <nx/vms/client/core/resource/resource_fwd.h>

namespace nx {
namespace client {
namespace mobile {

// TODO: #ynikitenkov Move to client core, use both in desktop and mobile client.
class PtzAvailabilityWatcher: public QObject
{
    Q_OBJECT
    using base_type = QObject;

    Q_PROPERTY(bool available READ available NOTIFY availabilityChanged)

public:
    PtzAvailabilityWatcher(
        const nx::vms::client::core::CameraPtr& camera,
        QObject* parent = nullptr);

    bool available() const;

signals:
    void availabilityChanged();

private:
    void setAvailable(bool value);
    void updateAvailability();

private:
    const nx::vms::client::core::CameraPtr m_camera;
    bool m_available = false;
};

} // namespace mobile
} // namespace client
} // namespace nx
