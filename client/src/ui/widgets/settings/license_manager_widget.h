#ifndef QN_LICENSE_MANAGER_WIDGET_H
#define QN_LICENSE_MANAGER_WIDGET_H

#include <QtGui/QWidget>
#include <QtCore/QModelIndex>

#include "licensing/license.h"
#include "api/app_server_connection.h"

class QNetworkAccessManager;

class QnLicenseListModel;

namespace Ui {
    class LicenseManagerWidget;
}

class QnLicenseManagerWidget : public QWidget {
    Q_OBJECT

public:
    explicit QnLicenseManagerWidget(QWidget *parent = 0);
    virtual ~QnLicenseManagerWidget();

private slots:
    void updateLicenses();

    void at_downloadError();
    void at_downloadFinished();
    void at_licensesReceived(int status, QnLicenseList licenses, int handle);

    void at_licenseDetailsButton_clicked();
    void at_gridLicenses_currentChanged();
    void at_gridLicenses_doubleClicked(const QModelIndex &index);
    void at_licenseWidget_stateChanged();

private:
    void updateFromServer(const QByteArray &licenseKey, const QString &hardwareId, const QString &oldHardwareId);
    void validateLicenses(const QByteArray& licenseKey, const QList<QnLicensePtr> &licenses);
    void showLicenseDetails(const QnLicensePtr &license);

private:
    Q_DISABLE_COPY(QnLicenseManagerWidget)

    QScopedPointer<Ui::LicenseManagerWidget> ui;
    QnLicenseListModel *m_model;
    QNetworkAccessManager *m_httpClient;
    QnLicenseList m_licenses;
	QMap<QNetworkReply*, QByteArray> m_replyKeyMap;
	QMap<int, QByteArray> m_handleKeyMap;
};

#endif // QN_LICENSE_MANAGER_WIDGET_H
