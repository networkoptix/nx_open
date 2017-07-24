#pragma once

#include <api/model/ping_reply.h>

class QnCommonModule;

namespace rest {
namespace helper {

class PingRestHelper
{
public:
    static QnPingReply data(const QnCommonModule* commonModule);
};

} // namespace helper
} // namespace rest
