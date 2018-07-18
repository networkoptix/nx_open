#pragma once

#include <plugins/resource/hanwha/hanwha_common.h>
#include <plugins/resource/hanwha/hanwha_attributes.h>
#include <plugins/resource/hanwha/hanwha_cgi_parameters.h>

namespace nx {
namespace mediaserver_core {
namespace plugins {

struct HanwhaInformation
{
    HanwhaDeviceType deviceType;
    QString firmware;
    QString macAddress;
    QString model;
    int channelCount = 0;
    HanwhaAttributes attributes;
    HanwhaCgiParameters cgiParameters;
};

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx
