#include "overlay_label_widget.h"

#include <QtWidgets/QLabel>
#include <QtWidgets/QVBoxLayout>

namespace nx {
namespace client {
namespace desktop {
namespace ui {

struct OverlayLabelWidget::Private
{
    Private(OverlayLabelWidget* parent):
        label(new QLabel(parent))
    {
        auto layout = new QVBoxLayout(parent);
        layout->setContentsMargins(QMargins());
        layout->addWidget(label);
    }

    QLabel* label;
};

OverlayLabelWidget::OverlayLabelWidget(QWidget* parent):
    base_type(parent),
    d(new Private(this))
{
}

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
