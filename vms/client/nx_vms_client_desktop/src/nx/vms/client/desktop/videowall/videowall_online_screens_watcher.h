// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <set>

#include <QtCore/QObject>

#include <nx/utils/uuid.h>

namespace nx::vms::common { class SystemContext; }

namespace nx::vms::client::desktop {

class VideoWallOnlineScreensWatcher: public QObject
{
    Q_OBJECT

public:
    VideoWallOnlineScreensWatcher(
        nx::vms::common::SystemContext* systemContext,
        QObject* parent = nullptr);

    std::set<nx::Uuid> onlineScreens() const;

signals:
    void onlineScreensChanged();

private:
    std::set<nx::Uuid> m_onlineScreens;
};

} // namespace nx::vms::client::desktop
