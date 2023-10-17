// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/desktop/common/widgets/tool_button.h>
#include <nx/vms/client/desktop/window_context_aware.h>

class QnCloudStatusPanelPrivate;

class QnCloudStatusPanel:
    public nx::vms::client::desktop::ToolButton,
    public nx::vms::client::desktop::WindowContextAware
{
    Q_OBJECT
    using base_type = nx::vms::client::desktop::ToolButton;

public:
    QnCloudStatusPanel(
        nx::vms::client::desktop::WindowContext* windowContext,
        QWidget* parent = nullptr);
    ~QnCloudStatusPanel();

    virtual QSize minimumSizeHint() const override;
    virtual QSize sizeHint() const override;

private:
    QScopedPointer<QnCloudStatusPanelPrivate> d_ptr;
    Q_DECLARE_PRIVATE(QnCloudStatusPanel)
};
