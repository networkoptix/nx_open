// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/impl_ptr.h>
#include <ui/dialogs/common/session_aware_dialog.h>

class QnSessionAwareDelegate;

namespace Ui { class QnSystemAdministrationDialog; }

class QnSystemAdministrationDialog: public QnSessionAwareTabbedDialog
{
    Q_OBJECT
    using base_type = QnSessionAwareTabbedDialog;

public:
    enum DialogPage
    {
        GeneralPage,
        UserManagement,
        UpdatesPage,
        LicensesPage,
        MailSettingsPage,
        SecurityPage,
        CloudManagement,
        TimeServerSelection,
        RoutingManagement,
        Analytics,

        PageCount
    };
    Q_ENUM(DialogPage)

    explicit QnSystemAdministrationDialog(QWidget* parent = nullptr);
    virtual ~QnSystemAdministrationDialog() override;

private:
    Q_DISABLE_COPY(QnSystemAdministrationDialog)
    nx::utils::ImplPtr<Ui::QnSystemAdministrationDialog> ui;
};
