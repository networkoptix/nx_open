#pragma once

#include <QtCore/QCoreApplication>

class QnCloudResultMessages final
{
    Q_DECLARE_TR_FUNCTIONS(QnCloudResultMessages)

public:
    QnCloudResultMessages() = delete;

    static QString invalidCredentials();
    static QString accountNotActivated();
};
