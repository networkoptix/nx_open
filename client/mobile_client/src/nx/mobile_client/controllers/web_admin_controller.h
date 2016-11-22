#pragma once

#include <QtCore/QObject>

class QnMobileClientUiController;

namespace nx {
namespace mobile_client {
namespace controllers {

class WebAdminController: public QObject
{
    Q_OBJECT

public:
    WebAdminController(QObject* parent = nullptr);

    void setUiController(QnMobileClientUiController* controller);

public slots:
    void openUrlInBrowser(const QString &urlString);
    QString getCredentials() const;
    void updateCredentials(const QString& login, const QString& password, bool isCloud);
    void cancel();

    bool liteClientMode() const;
    void startCamerasMode();

private:
    QnMobileClientUiController* m_uiController = nullptr;
};

} // namespace controllers
} // namespace mobile_client
} // namespace nx
