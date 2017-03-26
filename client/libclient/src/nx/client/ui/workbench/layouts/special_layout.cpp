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
    }

private:

};

} // namespace

namespace nx {
namespace client {
namespace ui {
namespace workbench {
namespace layouts {

SpecialLayout::SpecialLayout(const QnLayoutResourcePtr& resource, QObject* parent):
    base_type(resource, parent)
{
    setFlags(flags() | QnLayoutFlag::SpecialBackground);
    setPanelWidget(new SpecialLayoutPanelWidget());
}

} // namespace layouts
} // namespace workbench
} // namespace ui
} // namespace client
} // namespace nx

