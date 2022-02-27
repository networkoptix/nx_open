// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef PROCESS_UTILS_H
#define PROCESS_UTILS_H

#include <QtCore/QStringList>

class ProcessUtils
{
public:
    static bool startProcessDetached(const QString &program,
            const QStringList &arguments = QStringList(),
            const QString &workingDirectory = QString(),
            const QStringList &environment = QStringList());

    static void initialize();
};

#endif // PROCESS_UTILS_H
