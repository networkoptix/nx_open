// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QSet>

#include <core/resource/resource_fwd.h>
#include <nx/utils/impl_ptr.h>

namespace nx::vms::client::desktop {

class InvalidRecordingScheduleWatcher: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    explicit InvalidRecordingScheduleWatcher(QObject* parent = nullptr);
    virtual ~InvalidRecordingScheduleWatcher() override;

    QnVirtualCameraResourceSet camerasWithInvalidSchedule() const;

signals:
    void camerasChanged();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
