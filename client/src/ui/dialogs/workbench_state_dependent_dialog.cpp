#include "workbench_state_dependent_dialog.h"

#include <ui/workbench/watchers/workbench_safemode_watcher.h>

QnWorkbenchStateDependentButtonBoxDialog::QnWorkbenchStateDependentButtonBoxDialog(QWidget *parent /* = NULL*/, Qt::WindowFlags windowFlags /* = 0*/)
    : base_type(parent, windowFlags)
    , QnWorkbenchStateDelegate(parent)
    , m_safeModeWatcher(nullptr)
{
}

bool QnWorkbenchStateDependentButtonBoxDialog::tryClose(bool force) {
    base_type::reject();
    if (force)
        base_type::hide();
    return true;
}

void QnWorkbenchStateDependentButtonBoxDialog::forcedUpdate() {
    retranslateUi();
    tryClose(true);
}

void QnWorkbenchStateDependentButtonBoxDialog::makeReadOnlyModeAware(QList<QWidget*> disabledWidgets) {
    if (!m_safeModeWatcher)
        m_safeModeWatcher = new QnWorkbenchSafeModeWatcher(this);
    m_safeModeWatcher->addControlledWidgets(disabledWidgets);
}

void QnWorkbenchStateDependentButtonBoxDialog::makeReadOnlyModeAware(QWidget* disabledWidget) {
    if (!m_safeModeWatcher)
        m_safeModeWatcher = new QnWorkbenchSafeModeWatcher(this);
    m_safeModeWatcher->addControlledWidget(disabledWidget);
}

void QnWorkbenchStateDependentButtonBoxDialog::makeReadOnlyModeAware(QDialogButtonBox::StandardButton button) {
    if (!m_safeModeWatcher)
        m_safeModeWatcher = new QnWorkbenchSafeModeWatcher(this);
    m_safeModeWatcher->addControlledButton(button);
}
