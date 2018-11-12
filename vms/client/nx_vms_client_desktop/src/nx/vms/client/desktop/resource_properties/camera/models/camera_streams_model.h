#pragma once

#include <QtCore/QString>

namespace nx::vms::client::desktop {

struct CameraStreamsModel
{
    QString primaryStreamUrl;
    QString secondaryStreamUrl;
};

} // namespace nx::vms::client::desktop
