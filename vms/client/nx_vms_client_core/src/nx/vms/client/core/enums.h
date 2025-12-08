// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <common/common_globals.h>

namespace nx::vms::client::core {

Q_NAMESPACE_EXPORT(NX_VMS_CLIENT_CORE_API)

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

enum class ActivationType
{
    enterKey,
    singleClick,
    doubleClick,
    middleClick
};
Q_ENUM_NS(ActivationType)

} // namespace nx::vms::client::core
