#pragma once

#include <QtCore/QUrl>
#include <QtCore/QCoreApplication>

#include <nx/utils/uuid.h>
#include <mobile_client/mobile_client_meta_types.h>

class QCoreApplication;

struct QnMobileClientStartupParameters
{
    QnMobileClientStartupParameters();
    QnMobileClientStartupParameters(const QCoreApplication& application);

    QString qmlRoot;
    bool liteMode = false;
    QnUuid videowallInstanceGuid;
    QUrl url;
    bool testMode = false;
    QString initialTest;
    qint16 webSocketPort = 0;
    AutoLoginMode autoLoginMode = AutoLoginMode::Undefined;
    QString logLevel;
};

Q_DECLARE_METATYPE(QnMobileClientStartupParameters)
