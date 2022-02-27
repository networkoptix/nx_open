// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <QtCore/QScopedPointer>

#include <nx/utils/impl_ptr.h>
#include <nx/utils/url.h>
#include <nx/vms/api/data/module_information.h>
#include <nx/vms/discovery/manager.h>
#include <ui/dialogs/common/button_box_dialog.h>
#include <ui/workbench/workbench_context_aware.h>

namespace Ui { class LoginDialog; }

namespace nx::vms::client::desktop {

class LoginDialog:
    public QnButtonBoxDialog,
    public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QnButtonBoxDialog;

public:
    explicit LoginDialog(QWidget *parent);
    virtual ~LoginDialog() override;

    virtual void accept() override;
    virtual void reject() override;

protected:
    virtual void showEvent(QShowEvent* event) override;

private:
    void sendTestConnectionRequest(const nx::utils::Url& url);

    void updateAcceptibility();
    void updateFocus();
    void updateUsability();

    void at_testButton_clicked();

    void setupIntroView();

    nx::utils::Url currentUrl() const;
    bool isValid() const;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
    QScopedPointer<Ui::LoginDialog> ui;
};

} // namespace nx::vms::client::desktop
