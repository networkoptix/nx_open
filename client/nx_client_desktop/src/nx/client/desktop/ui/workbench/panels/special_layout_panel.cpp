#include "special_layout_panel.h"

#include <QtWidgets/QGraphicsWidget>

#include <ui/workbench/workbench.h>
#include <nx/client/desktop/ui/workbench/layouts/special_layout.h>
#include <utils/common/connective.h>

namespace nx {
namespace client {
namespace desktop {
namespace ui {
namespace workbench {

class SpecialLayoutPanelPrivate: public Connective<QObject>, public QnWorkbenchContextAware
{
    using base_type = Connective<QObject>;

    Q_DECLARE_PUBLIC(SpecialLayoutPanel)
    SpecialLayoutPanel* q_ptr = nullptr;

public:
    SpecialLayoutPanelPrivate(
        QGraphicsWidget* parentWidget,
        QObject* objectParent,
        SpecialLayoutPanel* parent);

    QGraphicsWidget* panelWidget() const;

    void handleCurrentLayoutChanged(QnWorkbenchLayout* layout);

private:
    void setPanelWidget(QGraphicsWidget* widget);
    void resetPanelWidget();

private:
    QGraphicsItem* const m_parentWidget = nullptr;
    QPointer<SpecialLayout> m_layout;
    QPointer<QGraphicsWidget> m_panelWidget;
};

SpecialLayoutPanelPrivate::SpecialLayoutPanelPrivate(
    QGraphicsWidget* parentWidget,
    QObject* objectParent,
    SpecialLayoutPanel* parent)
    :
    base_type(),
    QnWorkbenchContextAware(objectParent),
    q_ptr(parent),
    m_parentWidget(parentWidget)
{
    handleCurrentLayoutChanged(workbench()->currentLayout());
    connect(workbench(), &QnWorkbench::currentLayoutChanged, this,
        [this](){ handleCurrentLayoutChanged(workbench()->currentLayout());});
}

void SpecialLayoutPanelPrivate::handleCurrentLayoutChanged(QnWorkbenchLayout* layout)
{
    const auto specialLayout = qobject_cast<SpecialLayout*>(layout);
    if (m_layout == specialLayout)
        return;

    if (m_layout)
        disconnect(m_layout);

    m_layout = specialLayout;

    resetPanelWidget();
    if (!m_layout)
        return;

    const auto updatePanelWidget = [this]() { setPanelWidget(m_layout->panelWidget());};
    connect(m_layout, &SpecialLayout::panelWidgetChanged, this, updatePanelWidget);

    updatePanelWidget();
}

QGraphicsWidget* SpecialLayoutPanelPrivate::panelWidget() const
{
    return m_panelWidget;
}

void SpecialLayoutPanelPrivate::resetPanelWidget()
{
    setPanelWidget(nullptr);
}

void SpecialLayoutPanelPrivate::setPanelWidget(QGraphicsWidget* widget)
{
    if (m_panelWidget == widget)
        return;

    Q_Q(SpecialLayoutPanel);
    if (m_panelWidget)
    {
        m_panelWidget->setVisible(false);
        q->disconnect(m_panelWidget);
    }

    m_panelWidget = widget;

    if (m_panelWidget)
    {
        m_panelWidget->setParentItem(m_parentWidget);
        m_panelWidget->setVisible(true);
        connect(m_panelWidget, &QGraphicsWidget::geometryChanged,
            q, &NxUi::AbstractWorkbenchPanel::geometryChanged);
    }

    emit q->geometryChanged();
}

//-------------------------------------------------------------------------------------------------

SpecialLayoutPanel::SpecialLayoutPanel(
    const QnPaneSettings& settings,
    QGraphicsWidget* parentWidget,
    QObject* parent)
    :
    base_type(settings, parentWidget, parent),
    d_ptr(new SpecialLayoutPanelPrivate(parentWidget, parent, this))
{
}

SpecialLayoutPanel::~SpecialLayoutPanel()
{
}

QGraphicsWidget* SpecialLayoutPanel::widget()
{
    Q_D(const SpecialLayoutPanel);
    return d->panelWidget();
}

bool SpecialLayoutPanel::isPinned() const
{
    return true;
}

bool SpecialLayoutPanel::isOpened() const
{
    return true;
}

void SpecialLayoutPanel::setOpened(bool /* opened */, bool /* animate */)
{
}

bool SpecialLayoutPanel::isVisible() const
{
    Q_D(const SpecialLayoutPanel);
    return d->panelWidget();
}

void SpecialLayoutPanel::setVisible(bool /* visible */, bool /* animate */)
{
}

qreal SpecialLayoutPanel::opacity() const
{
    return 1.0;
}

void SpecialLayoutPanel::setOpacity(qreal /* opacity */ , bool /* animate */)
{
}

bool SpecialLayoutPanel::isHovered() const
{
    return false;
}

QRectF SpecialLayoutPanel::effectiveGeometry() const
{
    Q_D(const SpecialLayoutPanel);
    const auto widget = d->panelWidget();
    return widget ? widget->geometry() : QRectF();
}

void SpecialLayoutPanel::stopAnimations()
{
}

} // namespace workbench
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx

