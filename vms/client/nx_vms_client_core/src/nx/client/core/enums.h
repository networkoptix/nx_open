#pragma once

#include <common/common_globals.h>

namespace nx::vms::client::core {

class Enums: public QObject
{
    Q_OBJECT

public:
    enum ResourceFlag
    {
        MediaResourceFlag = Qn::media,
        ServerResourceFlag = Qn::server,
        WebPageResourceFlag = Qn::web_page,
    };
    Q_ENUM(ResourceFlag)
};

} // namespace nx::vms::client::core
