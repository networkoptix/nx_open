#pragma once

#include <QtCore/QCoreApplication>

class QnCloudResultMessages
{
    Q_DECLARE_TR_FUNCTIONS(QnCloudResultMessages)

public:
    static QString invalidCredentials()
    {
        return tr("Incorrect email or password");
    }

    static QString accountNotActivated()
    {
        return tr("Account isn't activated. Please check your email and follow provided instructions");
    }
};
