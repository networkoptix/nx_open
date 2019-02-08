#pragma once

#include <nx/vms/client/desktop/common/widgets/tool_button.h>
#include <ui/workbench/workbench_context_aware.h>

class QnCloudStatusPanelPrivate;

class QnCloudStatusPanel:
    public nx::vms::client::desktop::ToolButton,
    public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = nx::vms::client::desktop::ToolButton;

public:
    explicit QnCloudStatusPanel(QWidget* parent = nullptr);
    ~QnCloudStatusPanel();

    virtual QSize minimumSizeHint() const override;
    virtual QSize sizeHint() const override;

private:
    QScopedPointer<QnCloudStatusPanelPrivate> d_ptr;
    Q_DECLARE_PRIVATE(QnCloudStatusPanel)
};
