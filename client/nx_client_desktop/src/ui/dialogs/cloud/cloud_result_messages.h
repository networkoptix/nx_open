#pragma once

#include <QtCore/QCoreApplication>

class QnCloudResultMessages final
{
    Q_DECLARE_TR_FUNCTIONS(QnCloudResultMessages)

public:
    QnCloudResultMessages() = delete;

    static QString accountNotFound();
    static QString invalidPassword();
    static QString accountNotActivated();
    static QString userLockedOut();
};
