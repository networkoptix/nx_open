#ifndef QN_LICENSE_MANAGER_WIDGET_H
#define QN_LICENSE_MANAGER_WIDGET_H

#include <QtWidgets/QWidget>

#include <nx_ec/ec_api.h>

#include <licensing/license.h>

#include <ui/widgets/settings/abstract_preferences_widget.h>

#include <utils/common/connective.h>

class QModelIndex;
class QNetworkAccessManager;
class QNetworkReply;

class QnLicenseListModel;
class QnLicenseUsageHelper;

namespace Ui {
    class LicenseManagerWidget;
}

class QnLicenseManagerWidget : public Connective<QnAbstractPreferencesWidget> {
    Q_OBJECT
    typedef Connective<QnAbstractPreferencesWidget> base_type;

public:
    explicit QnLicenseManagerWidget(QWidget *parent = 0);
    virtual ~QnLicenseManagerWidget();

private slots:
    void updateLicenses();
    void updateDetailsButtonEnabled();

    void at_downloadError();
    void at_downloadFinished();
    void at_licensesReceived(int handle, ec2::ErrorCode errorCode, QnLicenseList licenses);
    void at_licenseDetailsButton_clicked();
    void at_removeButton_clicked();
    void at_gridLicenses_doubleClicked(const QModelIndex &index);
    void at_licenseWidget_stateChanged();

    void showMessage(const QString &title, const QString &message, bool warning);
    void at_licenseRemoved(int reqID, ec2::ErrorCode errorCode, QnLicensePtr license);

signals:
    void showMessageLater(const QString &title, const QString &message, bool warning);

private:
    void updateFromServer(const QByteArray &licenseKey, bool infoMode);
    void validateLicenses(const QByteArray& licenseKey, const QList<QnLicensePtr> &licenses);
    void showLicenseDetails(const QnLicensePtr &license);

private:
    Q_DISABLE_COPY(QnLicenseManagerWidget)

    QScopedPointer<Ui::LicenseManagerWidget> ui;
    QnLicenseListModel *m_model;
    QScopedPointer<QnLicenseUsageHelper> m_camerasUsageHelper;
    QScopedPointer<QnLicenseUsageHelper> m_videowallUsageHelper;
    QNetworkAccessManager *m_httpClient;
    QnLicenseList m_licenses;
    QMap<QNetworkReply*, QByteArray> m_replyKeyMap;
    QMap<int, QByteArray> m_handleKeyMap;
};

#endif // QN_LICENSE_MANAGER_WIDGET_H
