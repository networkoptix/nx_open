// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "confirmation_dialogs.h"

#include <nx/vms/client/desktop/settings/show_once_settings.h>
#include <ui/dialogs/common/session_aware_dialog.h>

namespace nx::vms::client::desktop::rules {

namespace {

void customizeDialog(
    QnSessionAwareMessageBox& dialog,
    QnMessageBox::Icon icon,
    const QString& text,
    const QString& informativeText,
    QDialogButtonBox::StandardButtons standardButtons,
    QDialogButtonBox::StandardButton defaultButton,
    bool isCheckboxEnabled)
{
    dialog.setIcon(icon);
    dialog.setText(text);
    dialog.setInformativeText(informativeText);
    dialog.setStandardButtons(standardButtons);
    dialog.setDefaultButton(defaultButton);
    dialog.setCheckBoxEnabled(isCheckboxEnabled);
}

} // namespace

bool ConfirmationDialogs::confirmDelete(QWidget* parent, size_t count)
{
    auto showOnce = showOnceSettings();
    if (showOnce->deleteRule())
        return true;

    QnSessionAwareMessageBox dialog{parent};
    customizeDialog(
        dialog,
        QnMessageBox::Icon::Question,
        tr("Delete %n Rules?", "", count),
        tr("This action cannot be undone"),
        QDialogButtonBox::Cancel,
        QDialogButtonBox::NoButton,
        /*isCheckboxEnabled*/ true);

    dialog.addCustomButton(
        QnMessageBoxCustomButton::Delete,
        QDialogButtonBox::AcceptRole,
        Qn::ButtonAccent::Warning);

    const auto result = dialog.exec();

    showOnce->deleteRule = dialog.isChecked();

    return result != QDialogButtonBox::Cancel;
}

bool ConfirmationDialogs::confirmReset(QWidget* parent)
{
    auto showOnce = showOnceSettings();
    if (showOnce->resetRules())
        return true;

    QnSessionAwareMessageBox dialog{parent};
    customizeDialog(
        dialog,
        QnMessageBox::Icon::Question,
        tr("Reset all rules to defaults?"),
        tr("This action cannot be undone"),
        QDialogButtonBox::Cancel,
        QDialogButtonBox::NoButton,
        /*isCheckboxEnabled*/ true);

    dialog.addCustomButton(
        QnMessageBoxCustomButton::Reset,
        QDialogButtonBox::AcceptRole,
        Qn::ButtonAccent::Warning);

    const auto result = dialog.exec();

    showOnce->resetRules = dialog.isChecked();

    return result != QDialogButtonBox::Cancel;
}

} // namespace nx::vms::client::desktop::rules
