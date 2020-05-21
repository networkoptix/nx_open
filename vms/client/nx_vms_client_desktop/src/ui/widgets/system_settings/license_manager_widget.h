#pragma once

#include <QtWidgets/QWidget>

#include <nx_ec/ec_api_fwd.h>
#include <licensing/license_fwd.h>
#include <ui/widgets/common/abstract_preferences_widget.h>
#include <utils/common/connective.h>
#include <nx/vms/client/desktop/license/license_helpers.h>

#include <ui/workbench/workbench_context_aware.h>
#include <ui/dialogs/common/message_box.h>

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
    explicit QnLicenseManagerWidget(QWidget* parent = nullptr);
    virtual ~QnLicenseManagerWidget() override;

    virtual void loadDataToUi() override;
    virtual bool hasChanges() const override;
    virtual void applyChanges() override;

protected:
    virtual void showEvent(QShowEvent *event) override;

private slots:
    void updateLicenses();
    void updateButtons();

    void at_downloadError();
    void at_licensesReceived(const QByteArray& licenseKey, ec2::ErrorCode errorCode,
        const QnLicenseList& licenses);
    void at_licenseWidget_stateChanged();

    void licenseDetailsRequested(const QModelIndex& index);

    void at_licenseRemoved(int reqID, ec2::ErrorCode errorCode, QnLicensePtr license);

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

    void showDeactivationErrorsDialog(
        const QnLicenseList& licenses,
        const nx::vms::client::desktop::license::DeactivationErrors& errors);

    void exportLicenses();

    void showActivationErrorMessage(QJsonObject errorMessage);

private:
    Q_DISABLE_COPY(QnLicenseManagerWidget)

    QScopedPointer<Ui::LicenseManagerWidget> ui;
    QnLicenseListModel* m_model {nullptr};
    QNetworkAccessManager* m_httpClient {nullptr};
    QPushButton* m_exportLicensesButton {nullptr};
    QnLicenseList m_licenses;
    QnLicenseValidator* m_validator {nullptr};
    bool m_isRemoveTakeAwayOperation = true;

    using RequestInfo = nx::vms::client::desktop::license::RequestInfo;
    RequestInfo m_deactivationReason;
};
