#ifndef LICENSEWIDGET_H
#define LICENSEWIDGET_H

#include <QtGui/QWidget>

#include "licensing/license.h"
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

protected:
    void changeEvent(QEvent *event);

private Q_SLOTS:
    void setOnlineActivation(bool online);

    void browseLicenseFileButtonClicked();
    void licenseDetailsButtonClicked();
    void activateLicenseButtonClicked();

    void downloadError();
    void downloadFinished();

private:
    void updateFromServer(const QString &licenseKey, const QString &hardwareId);

    void updateControls();
    void validateLicense(const QnLicensePtr &license);

private:
    Q_DISABLE_COPY(LicenseWidget)

    QScopedPointer<Ui::LicenseWidget> ui;

    QNetworkAccessManager *m_httpClient;
};

#endif // LICENSEWIDGET_H
