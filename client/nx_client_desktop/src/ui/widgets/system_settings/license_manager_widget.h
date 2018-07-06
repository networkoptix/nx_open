#pragma once

#include <QtWidgets/QWidget>

#include <nx_ec/ec_api_fwd.h>
#include <licensing/license_fwd.h>
#include <ui/widgets/common/abstract_preferences_widget.h>
#include <utils/common/connective.h>
#include <nx/client/desktop/license/license_helpers.h>

#include <ui/workbench/workbench_context_aware.h>

class QModelIndex;
class QNetworkAccessManager;
class QNetworkReply;
class QPushButton;

class QnLicenseListModel;
class QnLicenseValidator;

namespace Ui {
class LicenseManagerWidget;
}

class QnLicenseManagerWidget:
    public Connective<QnAbstractPreferencesWidget>,
    public QnWorkbenchContextAware
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
    void at_licensesReceived(const QByteArray& licenseKey, ec2::ErrorCode errorCode,
        const QnLicenseList& licenses);
    void at_licenseWidget_stateChanged();

    void licenseDetailsRequested(const QModelIndex& index);

    void at_licenseRemoved(int reqID, ec2::ErrorCode errorCode, QnLicensePtr license);

    void showMessage(
        QnMessageBoxIcon icon,
        const QString& text,
        const QString& extras,
        CopyToClipboardButton button);

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
    bool canDeactivateLicense(const QnLicensePtr &license) const;

    enum class ForceRemove
    {
        No,
        Yes
    };

    void takeAwaySelectedLicenses();

    void removeLicense(const QnLicensePtr& license, ForceRemove force);

    void deactivateLicenses(const QnLicenseList& licenses);

    bool confirmDeactivation(const QnLicenseList& licenses);

    QString getLicenseDescription(const QnLicensePtr& license) const;

    using DeactivationErrors = nx::client::desktop::license::Deactivator::LicenseErrorHash;
    void showDeactivationErrorsDialog(
        const QnLicenseList& licenses,
        const DeactivationErrors& errors);

    QString getDeactivationErrorCaption(
        int licensesCount,
        int errorsCount) const;

    QString getDeactivationErrorMessage(
        const QnLicenseList& licenses,
        const DeactivationErrors& errors) const;

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
    QnLicenseListModel* m_model {nullptr};
    QNetworkAccessManager* m_httpClient {nullptr};
    QPushButton* m_exportLicensesButton {nullptr};
    QnLicenseList m_licenses;
    QnLicenseValidator* m_validator {nullptr};
    bool m_isRemoveTakeAwayOperation = true;

    using RequestInfo = nx::client::desktop::license::RequestInfo;
    RequestInfo m_deactivationReason;
};
