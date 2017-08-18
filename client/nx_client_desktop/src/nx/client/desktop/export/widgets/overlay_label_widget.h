#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

namespace nx {
namespace client {
namespace desktop {
namespace ui {

class OverlayLabelWidget: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    OverlayLabelWidget(QWidget* parent = nullptr);

private:
    struct Private;
    QScopedPointer<Private> d;
};

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
