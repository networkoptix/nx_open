#ifndef LICENSEMANAGERWIDGET_H
#define LICENSEMANAGERWIDGET_H

#include <QtGui/QWidget>

#include "licensing/license.h"
#include "api/AppServerConnection.h"

class QNetworkAccessManager;

namespace Ui {
    class QnLicenseManagerWidget;
}

class QnLicenseManagerWidget : public QWidget
{
    Q_OBJECT

public:
    explicit QnLicenseManagerWidget(QWidget *parent = 0);
    virtual ~QnLicenseManagerWidget();

private slots:
    void updateLicenses();

    void at_downloadError();
    void at_downloadFinished();
    void at_licensesReceived(int status, const QByteArray &errorString, QnLicenseList licenses, int handle);

    void at_licenseDetailsButton_clicked();
    void at_gridLicenses_currentChanged();
    void at_licenseWidget_stateChanged();


private:
    void updateFromServer(const QString &licenseKey, const QString &hardwareId);
    void validateLicense(const QnLicensePtr &license);

private:
    Q_DISABLE_COPY(QnLicenseManagerWidget)

    QScopedPointer<Ui::LicenseManagerWidget> ui;
    QNetworkAccessManager *m_httpClient;
    QnLicenseList m_licenses;
};

#endif // LICENSEMANAGERWIDGET_H
