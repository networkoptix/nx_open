// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

// TODO: #sivanov Get rid of this class.
class QnEnvironment
{
public:
    static void showInGraphicalShell(const QString &path);

    static QString getUniqueFileName(const QString &dirName, const QString &baseName);
};
