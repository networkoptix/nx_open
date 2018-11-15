#pragma once

#include <ctime>

struct soap;

namespace nx {
namespace mediaserver_core {
namespace plugins {
namespace onvif {

int soapWsseAddUsernameTokenDigest(
    struct soap *soap,
    const char *id,
    const char *username,
    const char *password,
    time_t now);

} // namespace onvif
} // namespace mediaserver_core
} // namespace plugins
} // namespace nx
