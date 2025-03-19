// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtQml/QQmlParserStatus>

namespace nx::vms::client::core {

class NX_VMS_CLIENT_CORE_API ContextFromQmlHandler: public QQmlParserStatus
{
    Q_INTERFACES(QQmlParserStatus)

public:
    virtual void onContextReady();

private:
    virtual void componentComplete() override;
    virtual void classBegin() override;
};

} // namespace nx::vms::client::core
