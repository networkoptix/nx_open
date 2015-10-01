#include "workbench_safemode_watcher.h"

#include <common/common_module.h>

#include <ui/common/palette.h>
#include <ui/style/warning_style.h>

QnWorkbenchSafeModeWatcher::QnWorkbenchSafeModeWatcher(QWidget *parentWidget /*= nullptr*/) : QObject(parentWidget) 
    , m_parentWidget(parentWidget)
    , m_warnLabel(nullptr)
    , m_buttonBox(nullptr)
{
    connect(qnCommon, &QnCommonModule::readOnlyChanged, this, &QnWorkbenchSafeModeWatcher::updateReadOnlyMode);

    QList<QDialogButtonBox *> buttonBoxes = parentWidget->findChildren<QDialogButtonBox *>(QString(), Qt::FindDirectChildrenOnly);
    Q_ASSERT_X(buttonBoxes.size() == 1, Q_FUNC_INFO, "Invalid buttonBox count");

    if (buttonBoxes.isEmpty())
        buttonBoxes = parentWidget->findChildren<QDialogButtonBox *>(QString(), Qt::FindChildrenRecursively);

    if(buttonBoxes.empty()) {
        qnWarning("Button box dialog '%1' doesn't have a button box.", metaObject()->className());
        return;
    }

    m_buttonBox = buttonBoxes[0];

    if(buttonBoxes.size() > 1)
        qnWarning("Button box dialog '%1' has several button boxes.", metaObject()->className());
    
    QHBoxLayout* layout = qobject_cast<QHBoxLayout*>(m_buttonBox->layout());
    if (!layout)
        return;

    m_warnLabel = new QLabel(tr("System is in safe mode"), m_parentWidget);
    setWarningStyle(m_warnLabel);
    m_warnLabel->setVisible(false);

    layout->insertWidget(0, m_warnLabel);

    updateReadOnlyMode(qnCommon->isReadOnly());
}

void QnWorkbenchSafeModeWatcher::updateReadOnlyMode(bool readOnly) {
    if (m_warnLabel)
        m_warnLabel->setVisible(readOnly);
    for (QWidget* widget: m_controlledWidgets)
        widget->setEnabled(!readOnly);
}

void QnWorkbenchSafeModeWatcher::addControlledWidget(QWidget *widget) {
    m_controlledWidgets << widget;
}

void QnWorkbenchSafeModeWatcher::addControlledWidgets(QList<QWidget*> widgets) {
    m_controlledWidgets.append(widgets);
}

void QnWorkbenchSafeModeWatcher::addControlledButton(QDialogButtonBox::StandardButton button) {
    if (!m_buttonBox)
        return;
    auto buttonWidget = m_buttonBox->button(button);
    if (buttonWidget)
        m_controlledWidgets << buttonWidget;
}
