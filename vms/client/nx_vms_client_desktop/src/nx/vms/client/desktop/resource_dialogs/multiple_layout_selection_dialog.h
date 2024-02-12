// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <nx/utils/uuid.h>
#include <ui/dialogs/common/session_aware_dialog.h>

namespace Ui { class MultipleLayoutSelectionDialog; }

namespace nx::vms::client::desktop {

class ResourceSelectionWidget;

class MultipleLayoutSelectionDialog: public QnSessionAwareButtonBoxDialog
{
    Q_OBJECT
    using base_type = QnSessionAwareButtonBoxDialog;

public:
    static bool selectLayouts(UuidSet& selectedLayoutsIds, QWidget* parent);

private:
    MultipleLayoutSelectionDialog(const UuidSet& selectedLayoutsIds, QWidget* parent = nullptr);
    virtual ~MultipleLayoutSelectionDialog() override;

private:
    const std::unique_ptr<Ui::MultipleLayoutSelectionDialog> ui;
    ResourceSelectionWidget* m_resourceSelectionWidget;
};

} // namespace nx::vms::client::desktop
