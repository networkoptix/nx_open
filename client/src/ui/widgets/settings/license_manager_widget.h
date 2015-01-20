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
class QnLicenseUsageWatcher;

namespace Ui {
    class LicenseManagerWidget;
}

class QnLicenseManagerWidget : public Connective<QnAbstractPreferencesWidget> {
    Q_OBJECT
    typedef Connective<QnAbstractPreferencesWidget> base_type;

public:
    explicit QnLicenseManagerWidget(QWidget *parent = 0);
    virtual ~QnLicenseManagerWidget();

protected:
    virtual void showEvent(QShowEvent *event) override;

private slots:
    void updateLicenses();
    void updateDetailsButtonEnabled();

    void at_downloadError();
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
    void updateFromServer(const QByteArray &licenseKey, bool infoMode, const QUrl &url);
    void processReply(QNetworkReply *reply, const QByteArray& licenseKey, const QUrl &url, bool infoMode);
    void validateLicenses(const QByteArray& licenseKey, const QList<QnLicensePtr> &licenses);
    void showLicenseDetails(const QnLicensePtr &license);

private:
    Q_DISABLE_COPY(QnLicenseManagerWidget)

    QScopedPointer<Ui::LicenseManagerWidget> ui;
    QnLicenseListModel *m_model;
    QScopedPointer<QnLicenseUsageWatcher> m_camerasUsageWatcher;
    QScopedPointer<QnLicenseUsageWatcher> m_videowallUsageWatcher;
    QNetworkAccessManager *m_httpClient;
    QnLicenseList m_licenses;
    QMap<int, QByteArray> m_handleKeyMap;
};

#endif // QN_LICENSE_MANAGER_WIDGET_H
