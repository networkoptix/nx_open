#pragma once

#include <utils/common/id.h>

class QCoreApplication;

struct QnMobileClientStartupParameters
{
    QnMobileClientStartupParameters();
    QnMobileClientStartupParameters(const QCoreApplication& application);

    QString basePath;
    bool liteMode = false;
    QnUuid videowallInstanceGuid;
    QUrl url;
    bool testMode = false;
    QString initialTest;
    qint16 webSocketPort = 0;
};

Q_DECLARE_METATYPE(QnMobileClientStartupParameters)
