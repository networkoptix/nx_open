// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include "process/platform_process.h"

class NX_VMS_COMMON_API QnCorePlatformAbstraction: public QObject
{
public:
    QnCorePlatformAbstraction(QObject *parent = nullptr);
    virtual ~QnCorePlatformAbstraction();

    QnPlatformProcess *process(QProcess *source = NULL) const;

private:
    QnPlatformProcess *m_process;
};
