// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QAbstractButton>

#include <core/resource/resource_fwd.h>
#include <nx/vms/client/desktop/resource/resource_fwd.h>
#include <ui/graphics/items/generic/masked_proxy_widget.h>
#include <ui/workbench/workbench_context_aware.h>

namespace Ui{
class SpecialLayoutPanelWidget;
} // namespace Ui

namespace nx::vms::client::desktop {
namespace ui {
namespace workbench {

class SpecialLayoutPanelWidget:
    public QnMaskedProxyWidget,
    public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QnMaskedProxyWidget;

public:
    SpecialLayoutPanelWidget(const LayoutResourcePtr& layoutResource, QObject* parent = nullptr);

    virtual ~SpecialLayoutPanelWidget();

private:
    void handleResourceDataChanged(int role);

    void updateButtons();
    void updateTitle();

private:
    QScopedPointer<Ui::SpecialLayoutPanelWidget> ui;
    LayoutResourcePtr m_layoutResource;

    using ButtonPtr = QAbstractButton*;
    QHash<nx::vms::client::desktop::ui::action::IDType, ButtonPtr> m_actionButtons;
};

} // namespace workbench
} // namespace ui
} // namespace nx::vms::client::desktop
