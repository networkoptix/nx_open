// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <ui/dialogs/common/session_aware_dialog.h>

namespace Ui { class FailoverPriorityDialog; }

namespace nx::vms::client::desktop {

class FailoverPriorityViewWidget;

class FailoverPriorityDialog: public QnSessionAwareButtonBoxDialog
{
    Q_OBJECT
    using base_type = QnSessionAwareButtonBoxDialog;

public:
    FailoverPriorityDialog(QWidget* parent);
    virtual ~FailoverPriorityDialog() override;

protected:
    virtual void buttonBoxClicked(QDialogButtonBox::StandardButton button) override;

private:
    const std::unique_ptr<Ui::FailoverPriorityDialog> ui;
    std::function<void()> m_applyFailoverPriority;
};

} // namespace nx::vms::client::desktop
