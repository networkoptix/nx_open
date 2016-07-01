#pragma once

#include <QtWidgets/QWidget>

#include <nx_ec/ec_api.h>

#include <licensing/license.h>

#include <ui/widgets/common/abstract_preferences_widget.h>

#include <utils/common/connective.h>

class QModelIndex;
class QNetworkAccessManager;
class QNetworkReply;

class QnLicenseListModel;

namespace Ui {
class LicenseManagerWidget;
}

class QnLicenseManagerWidget : public Connective<QnAbstractPreferencesWidget>
{
    Q_OBJECT
    typedef Connective<QnAbstractPreferencesWidget> base_type;

public:
    explicit QnLicenseManagerWidget(QWidget *parent = 0);
    virtual ~QnLicenseManagerWidget();

    virtual void loadDataToUi() override;
    virtual bool hasChanges() const override;
    virtual void applyChanges() override;

protected:
    virtual void showEvent(QShowEvent *event) override;

private slots:
    void updateLicenses();
    void updateButtons();

    void at_downloadError();
    void at_licensesReceived(const QByteArray& licenseKey, ec2::ErrorCode errorCode, const QnLicenseList& licenses);
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

    QnLicenseList selectedLicenses() const;
    bool canRemoveLicense(const QnLicensePtr &license) const;

    void exportLicenses();

private:
    Q_DISABLE_COPY(QnLicenseManagerWidget)

    QScopedPointer<Ui::LicenseManagerWidget> ui;
    QnLicenseListModel *m_model;
    QNetworkAccessManager *m_httpClient;
    QnLicenseList m_licenses;
};
