// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <QtWidgets/QWidget>

#include <nx_ec/ec_api_fwd.h>
#include <licensing/license_fwd.h>
#include <ui/widgets/common/abstract_preferences_widget.h>
#include <nx/vms/client/desktop/license/license_helpers.h>

#include <ui/workbench/workbench_context_aware.h>
#include <ui/dialogs/common/message_box.h>
#include <nx/vms/license/license_usage_fwd.h>

class QModelIndex;
class QNetworkAccessManager;
class QNetworkReply;
class QPushButton;
class QnLicensePool;
class QnLicenseListModel;

namespace nx::vms::client::desktop { class LayoutWidgetHider; }

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

    void at_downloadError();
    void at_licenseWidget_stateChanged();

    void licenseDetailsRequested(const QModelIndex& index);

private:
    QnLicensePool* licensePool() const;
    QnUuid serverId() const;

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
    std::unique_ptr<nx::vms::client::desktop::LayoutWidgetHider> m_removeDetailsButtonsHider;
    QnLicenseList m_licenses;
    nx::vms::license::Validator* m_validator {nullptr};
    bool m_isRemoveTakeAwayOperation = true;

    using RequestInfo = nx::vms::client::desktop::license::RequestInfo;
    RequestInfo m_deactivationReason;
};
