#include "special_layout.h"

#include <QtWidgets/QGraphicsProxyWidget>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>

namespace {

class SpecialLayoutPanelWidget: public QGraphicsProxyWidget
{
public:
    SpecialLayoutPanelWidget()
    {
        auto body = new QWidget();
        body->setMinimumSize(1700, 300);
        const  auto layout = new QHBoxLayout(body);
        const auto text = new QLabel(body);
        layout->addWidget(text);
        layout->setAlignment(text, Qt::AlignHCenter);
        text->setText(lit("++++++++++++++++++++++++++++"));
        setWidget(body);

        setOpacity(0.5);
    }

private:

};

} // namespace

namespace nx {
namespace client {
namespace desktop {
namespace ui {
namespace workbench {

SpecialLayout::SpecialLayout(const QnLayoutResourcePtr& resource, QObject* parent):
    base_type(resource, parent)
{
    setFlags(flags() | QnLayoutFlag::SpecialBackground);
}

QnWorkbenchLayout::GraphicsWidgetPtr SpecialLayout::createPanelWidget() const
{
    return QnWorkbenchLayout::GraphicsWidgetPtr(new SpecialLayoutPanelWidget());
}


} // namespace workbench
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx

