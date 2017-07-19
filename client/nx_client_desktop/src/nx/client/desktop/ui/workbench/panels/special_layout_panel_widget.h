#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QAbstractButton>

#include <core/resource/resource_fwd.h>

#include <ui/graphics/items/generic/masked_proxy_widget.h>
#include <ui/workbench/workbench_context_aware.h>

#include <utils/common/connective.h>

namespace Ui{
class SpecialLayoutPanelWidget;
} // namespace Ui

namespace nx {
namespace client {
namespace desktop {
namespace ui {
namespace workbench {

class SpecialLayoutPanelWidget:
    public Connective<QnMaskedProxyWidget>,
    public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = Connective<QnMaskedProxyWidget>;

public:
    SpecialLayoutPanelWidget(const QnLayoutResourcePtr& layoutResource, QObject* parent = nullptr);

    virtual ~SpecialLayoutPanelWidget();

private:
    void handleResourceDataChanged(int role);

    void updateButtons();
    void updateTitle();

private:
    QScopedPointer<Ui::SpecialLayoutPanelWidget> ui;
    QnLayoutResourcePtr m_layoutResource;

    using ButtonPtr = QAbstractButton*;
    QHash<nx::client::desktop::ui::action::IDType, ButtonPtr> m_actionButtons;
};

} // namespace workbench
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
