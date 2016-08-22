#pragma once

#include <ui/widgets/common/tool_button.h>
#include <ui/workbench/workbench_context_aware.h>

class QnCloudStatusPanelPrivate;

class QnCloudStatusPanel: public QnToolButton, public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QnToolButton;

public:
    explicit QnCloudStatusPanel(QWidget* parent = nullptr);
    ~QnCloudStatusPanel();

private:
    QScopedPointer<QnCloudStatusPanelPrivate> d_ptr;
    Q_DECLARE_PRIVATE(QnCloudStatusPanel)
};
