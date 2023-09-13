// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_fwd.h>
#include <nx/utils/impl_ptr.h>
#include <ui/dialogs/common/session_aware_dialog.h>

namespace Ui { class ServerSettingsDialog; }

class QnServerSettingsDialog: public QnSessionAwareTabbedDialog
{
    Q_OBJECT
    using base_type = QnSessionAwareTabbedDialog;

public:
    enum DialogPage
    {
        SettingsPage,
        StatisticsPage,
        StorageManagmentPage,
        PoePage,
        PluginsPage,
        BackupPage,

        PageCount
    };

    QnServerSettingsDialog(QWidget* parent = nullptr);
    virtual ~QnServerSettingsDialog() override;

    QnMediaServerResourcePtr server() const;
    void setServer(const QnMediaServerResourcePtr& server);

protected:
    virtual void retranslateUi() override;
    virtual void applyChanges() override;
    virtual void discardChanges() override;
    virtual bool isNetworkRequestRunning() const override;

    bool switchServerWithConfirmation();
    virtual bool event(QEvent* event) override;

private:
    void updateWebPageLink();
    void setupShowWebServerLink();

private:
    struct Private;
    nx::utils::ImplPtr<Ui::ServerSettingsDialog> ui;
    nx::utils::ImplPtr<Private> d;
};
