// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <api/server_rest_connection_fwd.h>
#include <licensing/license_fwd.h>
#include <nx/vms/client/desktop/license/license_helpers.h>
#include <nx/vms/license/license_usage_fwd.h>
#include <ui/widgets/common/abstract_preferences_widget.h>
#include <ui/workbench/workbench_context_aware.h>

class QModelIndex;
class QPushButton;
class QnLicensePool;
class QnLicenseListModel;

namespace nx::vms::client::desktop {

namespace Ui { class LicenseManagerWidget; }

class LicenseManagerWidget: public QnAbstractPreferencesWidget, public QnWorkbenchContextAware
{
    Q_OBJECT

    using base_type = QnAbstractPreferencesWidget;

public:
    explicit LicenseManagerWidget(QWidget* parent = nullptr);
    virtual ~LicenseManagerWidget() override;

    virtual void loadDataToUi() override;
    virtual bool isNetworkRequestRunning() const override;
    virtual void discardChanges() override;

protected:
    virtual void showEvent(QShowEvent* event) override;

private:
    void updateLicenses();
    void updateButtons();

    void handleDownloadError();
    void handleWidgetStateChange();

    void licenseDetailsRequested(const QModelIndex& index);

private:
    QnLicensePool* licensePool() const;

    void showLicenseDetails(const QnLicensePtr& license);

    QnLicenseList selectedLicenses() const;
    bool canRemoveLicense(const QnLicensePtr& license) const;
    bool canDeactivateLicense(const QnLicensePtr& license) const;

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

private:
    QScopedPointer<Ui::LicenseManagerWidget> ui;
    QnLicenseListModel* m_model = nullptr;
    QPushButton* m_exportLicensesButton = nullptr;
    QnLicenseList m_licenses;
    nx::vms::license::Validator* m_validator = nullptr;
    bool m_isRemoveTakeAwayOperation = true;

    using RequestInfo = nx::vms::client::desktop::license::RequestInfo;
    RequestInfo m_deactivationReason;
    rest::Handle m_currentRequest = 0;
};

} // namespace nx::vms::client::desktop
