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

    for (const ControlledWidget &widget: m_controlledWidgets) {
        switch (widget.mode) {
        case ControlMode::Hide:
            widget.widget->setVisible(!readOnly);
            break;
        case ControlMode::Disable:
            widget.widget->setEnabled(!readOnly);
            break;
        case ControlMode::MakeReadOnly:
            setReadOnly(widget.widget, readOnly);
            break;
        }
    }
}
        

void QnWorkbenchSafeModeWatcher::addWarningLabel(QDialogButtonBox *buttonBox, QWidget *beforeWidget) {
    QHBoxLayout* layout = qobject_cast<QHBoxLayout*>(buttonBox->layout());
    Q_ASSERT_X(layout, Q_FUNC_INFO, "Layout must already exist here.");
    if (!layout)
        return;

    if (!beforeWidget)
        beforeWidget = buttonBox->button(QDialogButtonBox::Ok);
    Q_ASSERT_X(beforeWidget, Q_FUNC_INFO, "Select correct widget to place");
    int index = beforeWidget
        ? layout->indexOf(beforeWidget)
        : 0;

    m_warnLabel = new QLabel(tr("System is in safe mode"), m_parentWidget);
    setWarningStyle(m_warnLabel);
    m_warnLabel->setVisible(false);

    layout->insertWidget(index, m_warnLabel);

    updateReadOnlyMode();
}

void QnWorkbenchSafeModeWatcher::addControlledWidget(QWidget *widget, ControlMode mode) {
    m_controlledWidgets << ControlledWidget(widget, mode);
    updateReadOnlyMode();
}
