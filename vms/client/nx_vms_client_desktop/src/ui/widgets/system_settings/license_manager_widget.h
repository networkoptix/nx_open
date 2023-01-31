// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <licensing/license_fwd.h>
#include <nx/vms/client/desktop/license/license_helpers.h>
#include <nx/vms/license/license_usage_fwd.h>
#include <ui/widgets/common/abstract_preferences_widget.h>
#include <nx/vms/client/desktop/license/license_helpers.h>

#include <ui/workbench/workbench_context_aware.h>
#include <ui/dialogs/common/message_box.h>
#include <nx/vms/license/license_usage_fwd.h>

class QModelIndex;
class QPushButton;
class QnLicensePool;
class QnLicenseListModel;

namespace nx::network::http { class AsyncClient; }

namespace Ui {
class LicenseManagerWidget;
}

class QnLicenseManagerWidget:
    public QnAbstractPreferencesWidget,
    public QnWorkbenchContextAware
{
    Q_OBJECT
    typedef QnAbstractPreferencesWidget base_type;

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

    void at_licenseWidget_stateChanged();

    void licenseDetailsRequested(const QModelIndex& index);

private:
    QnLicensePool* licensePool() const;
    QnUuid serverId() const;

    void updateFromServer(const QByteArray& licenseKey, bool infoMode, const nx::utils::Url& url);
    void processReply(
        const QByteArray& licenseKey,
        const QByteArray& replyData,
        const nx::utils::Url& url,
        bool infoMode);
    void handleDownloadError();
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
    std::unique_ptr<nx::network::http::AsyncClient> m_httpClient;
    QPushButton* m_exportLicensesButton {nullptr};
    QnLicenseList m_licenses;
    nx::vms::license::Validator* m_validator {nullptr};
    bool m_isRemoveTakeAwayOperation = true;

    using RequestInfo = nx::vms::client::desktop::license::RequestInfo;
    RequestInfo m_deactivationReason;
};
