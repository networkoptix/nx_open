// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QCoreApplication>
#include <QtCore/QUrl>

#include <mobile_client/mobile_client_meta_types.h>
#include <nx/utils/url.h>
#include <nx/utils/uuid.h>

class QCoreApplication;

struct QnMobileClientStartupParameters
{
    QnMobileClientStartupParameters();
    QnMobileClientStartupParameters(const QCoreApplication& application);

    QString qmlRoot;
    QStringList qmlImportPaths;
    nx::utils::Url url;
    QString logLevel;
};

Q_DECLARE_METATYPE(QnMobileClientStartupParameters)
