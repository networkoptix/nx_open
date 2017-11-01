#pragma once

#include <QtCore/QUrl>
#include <QtCore/QCoreApplication>

#include <nx/utils/uuid.h>
#include <nx/utils/url.h>
#include <mobile_client/mobile_client_meta_types.h>

class QCoreApplication;

struct QnMobileClientStartupParameters
{
    QnMobileClientStartupParameters();
    QnMobileClientStartupParameters(const QCoreApplication& application);

    QString basePath;
    bool liteMode = false;
    QnUuid videowallInstanceGuid;
    nx::utils::Url url;
    bool testMode = false;
    QString initialTest;
    qint16 webSocketPort = 0;
    AutoLoginMode autoLoginMode = AutoLoginMode::Undefined;
    QString logLevel;
};

Q_DECLARE_METATYPE(QnMobileClientStartupParameters)
