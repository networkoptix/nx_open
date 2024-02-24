// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_fwd.h>
#include <nx/vms/client/desktop/common/dialogs/generic_tabbed_dialog.h>

namespace Ui { class LayoutSettingsDialog; }

namespace nx::vms::client::desktop {

struct LayoutSettingsDialogState;

class LayoutSettingsDialog: public GenericTabbedDialog
{
    Q_OBJECT
    using base_type = GenericTabbedDialog;

    enum class Tab
    {
        general,
        background,
    };

public:
    explicit LayoutSettingsDialog(QWidget* parent = nullptr);
    virtual ~LayoutSettingsDialog() override;

    bool setLayout(const QnLayoutResourcePtr& layout);

    virtual void accept() override;

private:
    void loadState(const LayoutSettingsDialogState& state);

private:
    Q_DISABLE_COPY(LayoutSettingsDialog)
    QScopedPointer<Ui::LayoutSettingsDialog> ui;

    struct Private;
    QScopedPointer<Private> d;
};

} // namespace nx::vms::client::desktop
