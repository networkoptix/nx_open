// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <ui/dialogs/common/button_box_dialog.h>

#include <ui/workbench/workbench_context_aware.h>

namespace Ui
{
    class ChangeUserPasswordDialog;
}

class QnChangeUserPasswordDialog: public QnButtonBoxDialog, public QnWorkbenchContextAware
{
    Q_OBJECT

    typedef QnButtonBoxDialog base_type;

public:
    QnChangeUserPasswordDialog(QWidget* parent);
    virtual ~QnChangeUserPasswordDialog();

    virtual void accept() override;

    QString newPassword() const;

    void setInfoText(const QString& text);

private:
    bool validate();

private:
    Q_DISABLE_COPY(QnChangeUserPasswordDialog)
    QScopedPointer<Ui::ChangeUserPasswordDialog> ui;
};


