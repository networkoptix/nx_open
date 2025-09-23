// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QCoreApplication>
#include <QtCore/QRect>
#include <QtCore/QUrl>

#include <nx/utils/url.h>
#include <nx/utils/uuid.h>

class QCoreApplication;

struct QnMobileClientStartupParameters
{
    QnMobileClientStartupParameters();
    QnMobileClientStartupParameters(const QCoreApplication& application);

    QString qmlRoot;
    QStringList qmlImportPaths;
    nx::Url url;
    QString logLevel;
    QRect windowGeometry;
};

Q_DECLARE_METATYPE(QnMobileClientStartupParameters)
