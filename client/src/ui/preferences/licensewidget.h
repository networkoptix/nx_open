#ifndef LICENSEWIDGET_H
#define LICENSEWIDGET_H

#include <QtGui/QWidget>

#include "licensing/license.h"
#include "api/AppServerConnection.h"
#include "ui_licensewidget.h"

class QNetworkAccessManager;

namespace Ui {
    class LicenseWidget;
}

class LicenseWidget : public QWidget
{
    Q_OBJECT

public:
    explicit LicenseWidget(QWidget *parent = 0);
    ~LicenseWidget();

    void setManager(QObject* manager);
    void setHardwareId(const QByteArray&);

protected:
    void changeEvent(QEvent *event);

private slots:
    void setOnlineActivation(bool online);

    void browseLicenseFileButtonClicked();
    void activateLicenseButtonClicked();
    void activateFreeLicenseButtonClicked();

    void downloadError();
    void downloadFinished();

    void serialKeyChanged(QString newText);

private:
    void updateFromServer(const QString &licenseKey, const QString &hardwareId);

    void updateControls();
    void validateLicense(const QnLicensePtr &license);

private:
    Q_DISABLE_COPY(LicenseWidget)

    QScopedPointer<Ui::LicenseWidget> ui;

    QObject* m_manager;
    QNetworkAccessManager *m_httpClient;
    QnAppServerConnectionPtr m_connection;

    friend class LicenseManagerWidget;
};

#endif // LICENSEWIDGET_H
