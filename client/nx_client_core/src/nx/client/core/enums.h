#pragma once

#include <common/common_globals.h>

namespace nx {
namespace client {
namespace core {

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

} // namespace core
} // namespace client
} // namespace nx
