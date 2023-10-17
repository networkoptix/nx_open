// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QAbstractButton>

#include <core/resource/resource_fwd.h>
#include <nx/vms/client/desktop/resource/resource_fwd.h>
#include <nx/vms/client/desktop/window_context_aware.h>
#include <ui/graphics/items/generic/masked_proxy_widget.h>

namespace Ui{
class SpecialLayoutPanelWidget;
} // namespace Ui

namespace nx::vms::client::desktop {
namespace ui {
namespace workbench {

class SpecialLayoutPanelWidget: public QnMaskedProxyWidget, public WindowContextAware
{
    Q_OBJECT
    using base_type = QnMaskedProxyWidget;

public:
    SpecialLayoutPanelWidget(
        WindowContext* windowContext,
        const LayoutResourcePtr& layoutResource,
        QObject* parent = nullptr);

    virtual ~SpecialLayoutPanelWidget();

private:
    void handleResourceDataChanged(int role);

    void updateButtons();
    void updateTitle();

private:
    QScopedPointer<Ui::SpecialLayoutPanelWidget> ui;
    LayoutResourcePtr m_layoutResource;

    using ButtonPtr = QAbstractButton*;
    QHash<nx::vms::client::desktop::menu::IDType, ButtonPtr> m_actionButtons;
};

} // namespace workbench
} // namespace ui
} // namespace nx::vms::client::desktop
