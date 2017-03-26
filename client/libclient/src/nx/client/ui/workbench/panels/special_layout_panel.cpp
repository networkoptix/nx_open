#include "special_layout_panel.h"

#include <QtWidgets/QGraphicsWidget>

#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_layout.h>
#include <nx/client/ui/workbench/layouts/special_layout.h>

namespace nx {
namespace client {
namespace ui {
namespace workbench {
namespace panels {

class SpecialLayoutPanel::PanelPrivate: public QObject, public QnWorkbenchContextAware
{
    using base_type = QObject;

public:
    PanelPrivate(QGraphicsItem* parentItem, QObject* parent);

    QGraphicsWidget* panelWidget() const;

    void handleCurrentLayoutChanged(QnWorkbenchLayout* layout);

private:
    void setPanelWidget(QGraphicsWidget* widget);
    void resetPanelWidget();

private:
    QGraphicsItem* const m_parentItem = nullptr;
    QnWorkbenchLayout* m_layout = nullptr;
    QPointer<QGraphicsWidget> m_panelWidget;
};

SpecialLayoutPanel::PanelPrivate::PanelPrivate(QGraphicsItem* parentItem, QObject* parent):
    base_type(),
    QnWorkbenchContextAware(parent),
    m_parentItem(parentItem)
{
    handleCurrentLayoutChanged(workbench()->currentLayout());
    connect(workbench(), &QnWorkbench::currentLayoutChanged, this,
        [this](){ handleCurrentLayoutChanged(workbench()->currentLayout());});
    }

void SpecialLayoutPanel::PanelPrivate::handleCurrentLayoutChanged(QnWorkbenchLayout* layout)
{
    const auto specialLayout = qobject_cast<layouts::SpecialLayout*>(layout);
    if (m_layout == specialLayout)
        return;

    if (m_layout)
        disconnect(m_layout);

    m_layout = specialLayout;

    resetPanelWidget();
    if (!m_layout)
        return;

    setPanelWidget(m_layout->panelWidget());
    connect(m_layout, &QnWorkbenchLayout::panelWidgetChanged, this,
        [this](){ setPanelWidget(m_layout->panelWidget());});
}

QGraphicsWidget* SpecialLayoutPanel::PanelPrivate::panelWidget() const
{
    return m_panelWidget;
}

void SpecialLayoutPanel::PanelPrivate::resetPanelWidget()
{
    setPanelWidget(nullptr);
}

void SpecialLayoutPanel::PanelPrivate::setPanelWidget(QGraphicsWidget* widget)
{
    if (m_panelWidget == widget)
        return;

    if (m_panelWidget)
        m_panelWidget->setParentItem(nullptr);

    m_panelWidget = widget;

    if (m_panelWidget)
        m_panelWidget->setParentItem(m_parentItem);
}

//-------------------------------------------------------------------------------------------------

SpecialLayoutPanel::SpecialLayoutPanel(
    const QnPaneSettings& settings,
    QGraphicsWidget* parentWidget,
    QObject* parent)
    :
    base_type(settings, parentWidget, parent),
    d(new PanelPrivate(parentWidget, parent))
{
}

bool SpecialLayoutPanel::isPinned() const
{
    return true;
}

bool SpecialLayoutPanel::isOpened() const
{
    return d->panelWidget();
}

void SpecialLayoutPanel::setOpened(bool opened, bool animate)
{

}

bool SpecialLayoutPanel::isVisible() const
{
    return d->panelWidget();
}

void SpecialLayoutPanel::setVisible(bool visible, bool animate)
{

}

qreal SpecialLayoutPanel::opacity() const
{
    return 1.0;
}

void SpecialLayoutPanel::setOpacity(qreal opacity, bool animate)
{

}

bool SpecialLayoutPanel::isHovered() const
{
    return false;
}

QRectF SpecialLayoutPanel::effectiveGeometry() const
{
    return QRectF();
}

void SpecialLayoutPanel::stopAnimations()
{
}

} // namespace panels
} // namespace workbench
} // namespace ui
} // namespace client
} // namespace nx

