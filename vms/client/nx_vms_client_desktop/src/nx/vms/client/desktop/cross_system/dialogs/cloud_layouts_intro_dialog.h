// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/impl_ptr.h>
#include <ui/dialogs/common/button_box_dialog.h>

namespace Ui { class CloudLayoutsIntroDialog; }

namespace nx::vms::client::desktop {

class CloudLayoutsIntroDialog: public QnButtonBoxDialog
{
    Q_OBJECT
    using base_type = QnButtonBoxDialog;

public:
    enum class Mode
    {
        confirmation,
        info
    };

    explicit CloudLayoutsIntroDialog(
        Mode mode = Mode::confirmation,
        QWidget* parent = nullptr,
        Qt::WindowFlags windowFlags = {Qt::Dialog | Qt::MSWindowsFixedSizeDialogHint});

    virtual ~CloudLayoutsIntroDialog() override;

    bool doNotShowAgainChecked() const;

private:
    nx::utils::ImplPtr<Ui::CloudLayoutsIntroDialog> ui;
};

} // namespace nx::vms::client::desktop
