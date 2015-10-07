#include "workbench_safemode_watcher.h"

#include <common/common_module.h>

#include <ui/common/palette.h>
#include <ui/common/read_only.h>
#include <ui/style/warning_style.h>

QnWorkbenchSafeModeWatcher::QnWorkbenchSafeModeWatcher(QWidget *parentWidget /*= nullptr*/) : QObject(parentWidget) 
    , m_parentWidget(parentWidget)
    , m_warnLabel(nullptr)
{
    connect(qnCommon, &QnCommonModule::readOnlyChanged, this, &QnWorkbenchSafeModeWatcher::updateReadOnlyMode);
}

void QnWorkbenchSafeModeWatcher::updateReadOnlyMode() {
    bool readOnly = qnCommon->isReadOnly();

    if (m_warnLabel)
        m_warnLabel->setVisible(readOnly);

    for (QWidget* widget: m_autoHiddenWidgets)
        widget->setVisible(!readOnly);

    using ::setReadOnly;
    for (QWidget* widget: m_autoReadOnlyWidgets)
        setReadOnly(widget, readOnly);
}

void QnWorkbenchSafeModeWatcher::addWarningLabel(QDialogButtonBox *buttonBox) {
    QHBoxLayout* layout = qobject_cast<QHBoxLayout*>(buttonBox->layout());
    if (!layout)
        return;

    m_warnLabel = new QLabel(tr("System is in safe mode"), m_parentWidget);
    setWarningStyle(m_warnLabel);
    m_warnLabel->setVisible(false);

    layout->insertWidget(0, m_warnLabel);

    updateReadOnlyMode();
}

void QnWorkbenchSafeModeWatcher::addAutoHiddenWidget(QWidget *widget) {
    m_autoHiddenWidgets << widget;
    updateReadOnlyMode();
}

void QnWorkbenchSafeModeWatcher::addAutoReadOnlyWidget(QWidget *widget) {
    m_autoReadOnlyWidgets << widget;
    updateReadOnlyMode();
}
