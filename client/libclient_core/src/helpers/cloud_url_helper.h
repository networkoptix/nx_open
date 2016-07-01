#pragma once

#include <QtCore/QUrl>

class QnCloudUrlHelper
{
public:
    static QUrl mainUrl();
    static QUrl aboutUrl();
    static QUrl accountManagementUrl();
    static QUrl createAccountUrl();
    static QUrl restorePasswordUrl();
};
