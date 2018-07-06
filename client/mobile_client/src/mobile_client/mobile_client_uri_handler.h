#pragma once

#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <nx/utils/url.h>

class QnMobileClientUiController;
class QnMobileClientUriHandler : public QObject
{
    Q_OBJECT

public:
    explicit QnMobileClientUriHandler(QObject* parent = nullptr);

    void setUiController(QnMobileClientUiController* uiController);

    static QStringList supportedSchemes();
    static const char* handlerMethodName();

public slots:
    void handleUrl(const nx::utils::Url& url);

private:
    QPointer<QnMobileClientUiController> m_uiController;
};
