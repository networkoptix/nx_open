// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "core_platform_abstraction.h"

#ifndef QT_NO_PROCESS

#include <QtCore/QCoreApplication>
#include <QtCore/QProcess>

#include <nx/utils/log/log.h>

#if defined(Q_OS_WIN)
#   include "process/platform_process_win.h"
#   define QnProcessImpl QnWindowsProcess
#elif defined(Q_OS_LINUX)
#   include "process/platform_process_unix.h"
#   define QnProcessImpl QnUnixProcess
#elif defined(Q_OS_MACOS)
#   include "process/platform_process_unix.h"
#   define QnProcessImpl QnUnixProcess
#else
#   include "process/platform_process_unix.h"
#   define QnProcessImpl QnUnixProcess
#endif

QnCorePlatformAbstraction::QnCorePlatformAbstraction(QObject *parent):
    QObject(parent)
{
    NX_ASSERT(qApp, "QApplication instance must be created before a QnCorePlatformAbstraction.");

    m_process = new QnProcessImpl(NULL, this);
}

QnCorePlatformAbstraction::~QnCorePlatformAbstraction()
{
    return;
}

QnPlatformProcess *QnCorePlatformAbstraction::process(QProcess *source) const {
    if (source == nullptr)
        return m_process;

    static const char* qn_platformProcessPropertyName = "_qn_platformProcess";
    QnPlatformProcess* result =
        source->property(qn_platformProcessPropertyName).value<QnPlatformProcess *>();
    if (!result) {
        result = new QnProcessImpl(source, source);
        source->setProperty(qn_platformProcessPropertyName,
            QVariant::fromValue<QnPlatformProcess *>(result));
    }

    return result;
}

#endif // QT_NO_PROCESS
