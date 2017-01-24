#pragma once

#include <QtWidgets/QWidget>

#include <nx_ec/ec_api.h>

#include <licensing/license.h>

#include <ui/widgets/common/abstract_preferences_widget.h>

#include <utils/common/connective.h>

class QModelIndex;
class QNetworkAccessManager;
class QNetworkReply;
class QPushButton;

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

private:
    enum class CopyToClipboardButton
    {
        Hide,
        Show
    };

private slots:
    void updateLicenses();
    void updateButtons();

    void at_downloadError();
    void at_licensesReceived(const QByteArray& licenseKey, ec2::ErrorCode errorCode, const QnLicenseList& licenses);
    void at_licenseWidget_stateChanged();

    void licenseDetailsRequested(const QModelIndex& index);

    void showMessage(
        QnMessageBoxIcon icon,
        const QString& text,
        const QString& extras,
        CopyToClipboardButton button);

    void at_licenseRemoved(int reqID, ec2::ErrorCode errorCode, QnLicensePtr license);

signals:
    void showMessageLater(
        QnMessageBoxIcon icon,
        const QString& text,
        const QString& extras,
        CopyToClipboardButton button);

private:
    void updateFromServer(const QByteArray &licenseKey, bool infoMode, const QUrl &url);
    void processReply(QNetworkReply *reply, const QByteArray& licenseKey, const QUrl &url, bool infoMode);
    void validateLicenses(const QByteArray& licenseKey, const QList<QnLicensePtr> &licenses);
    void showLicenseDetails(const QnLicensePtr &license);

    QnLicenseList selectedLicenses() const;
    bool canRemoveLicense(const QnLicensePtr &license) const;
    void removeSelectedLicenses();

    void exportLicenses();

    static QString networkErrorText();
    static QString networkErrorExtras();
    static QString getContactSupportMessage();
    static QString getProblemPersistMessage();

    void showFailedToActivateLicenseLater(const QString& extras);
    void showIncompatibleLicenceMessageLater();
    void showActivationMessageLater(const QJsonObject& errorMessage);
    void showAlreadyActivatedLater(
        const QString& hwid,
        const QString& time = QString());

private:
    Q_DISABLE_COPY(QnLicenseManagerWidget)

    QScopedPointer<Ui::LicenseManagerWidget> ui;
    QnLicenseListModel* m_model;
    QNetworkAccessManager* m_httpClient;
    QPushButton* m_exportLicensesButton;
    QnLicenseList m_licenses;
};
