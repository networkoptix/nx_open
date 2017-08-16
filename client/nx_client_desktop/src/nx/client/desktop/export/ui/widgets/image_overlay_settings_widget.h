#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

namespace Ui { class ImageOverlaySettingsWidget; }

namespace nx {
namespace client {
namespace desktop {
namespace ui {

class ImageOverlaySettingsWidget: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    ImageOverlaySettingsWidget(QWidget* parent = nullptr);

private:
    QScopedPointer<Ui::ImageOverlaySettingsWidget> ui;
};

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
