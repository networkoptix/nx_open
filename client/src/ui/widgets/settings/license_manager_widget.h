#ifndef QN_LICENSE_MANAGER_WIDGET_H
#define QN_LICENSE_MANAGER_WIDGET_H

#include <QtWidgets/QWidget>
#include <QtCore/QModelIndex>

#include "licensing/license.h"
#include "api/app_server_connection.h"
#include <ui/widgets/settings/abstract_preferences_widget.h>

class QNetworkAccessManager;

class QnLicenseListModel;

namespace Ui {
    class LicenseManagerWidget;
}

class QnLicenseManagerWidget : public QnAbstractPreferencesWidget {
    Q_OBJECT
    typedef QnAbstractPreferencesWidget base_type;

public:
    explicit QnLicenseManagerWidget(QWidget *parent = 0);
    virtual ~QnLicenseManagerWidget();

private slots:
    void updateLicenses();
    void updateDetailsButtonEnabled();

    void at_downloadError();
    void at_downloadFinished();
    void at_licensesReceived(int status, QnLicenseList licenses, int handle);
    void at_licenseDetailsButton_clicked();
    void at_gridLicenses_doubleClicked(const QModelIndex &index);
    void at_licenseWidget_stateChanged();

    void showMessage(const QString &title, const QString &message, bool warning);

signals:
    void showMessageLater(const QString &title, const QString &message, bool warning);

private:
    void updateFromServer(const QByteArray &licenseKey, const QList<QByteArray> &mainHardwareIds, const QList<QByteArray> &compatibleHardwareIds);
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
