#include "special_layout.h"

#include <QtWidgets/QGraphicsProxyWidget>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>

namespace {

class SpecialLayoutPanelWidget: public QGraphicsProxyWidget
{
    using base_type = QGraphicsProxyWidget;

public:
    SpecialLayoutPanelWidget();

    void setCaption(const QString& caption);

private:
    QLabel* m_captionLabel;
};

SpecialLayoutPanelWidget::SpecialLayoutPanelWidget():
    base_type(),
    m_captionLabel(new QLabel())
{
    const auto body = new QWidget();

    const auto layout = new QHBoxLayout(body);
    layout->setContentsMargins(10, 10, 10, 10);

    layout->addWidget(m_captionLabel);

    setWidget(body);
}

void SpecialLayoutPanelWidget::setCaption(const QString& caption)
{
    m_captionLabel->setText(caption);
}

} // namespace

namespace nx {
namespace client {
namespace desktop {
namespace ui {
namespace workbench {

class SpecialLayout::SpecialLayoutPrivate
{
public:
    void updateCurrentPanelWidget(SpecialLayoutPanelWidget* widget);

    void setCaption(const QString& caption);

private:
    QPointer<SpecialLayoutPanelWidget> m_widget;
    QString m_caption;
};

void SpecialLayout::SpecialLayoutPrivate::updateCurrentPanelWidget(SpecialLayoutPanelWidget* widget)
{
    if (m_widget == widget)
        return;

    m_widget = widget;
    if (!m_widget)
        return;

    m_widget->setCaption(m_caption);
}

void SpecialLayout::SpecialLayoutPrivate::setCaption(const QString& caption)
{
    if (m_caption == caption)
        return;

    m_caption = caption;
    if (m_widget)
        m_widget->setCaption(caption);
}

//-------------------------------------------------------------------------------------------------

SpecialLayout::SpecialLayout(const QnLayoutResourcePtr& resource, QObject* parent):
    base_type(resource, parent),
    d(new SpecialLayoutPrivate())
{
    setFlags(flags() | QnLayoutFlag::SpecialBackground);
}

SpecialLayout::~SpecialLayout()
{
}

QnWorkbenchLayout::GraphicsWidgetPtr SpecialLayout::createPanelWidget() const
{
    const auto widget = new SpecialLayoutPanelWidget();
    d->updateCurrentPanelWidget(widget);
    return QnWorkbenchLayout::GraphicsWidgetPtr(widget);
}

void SpecialLayout::setPanelCaption(const QString& caption)
{
    d->setCaption(caption);
}


} // namespace workbench
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx

