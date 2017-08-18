#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

namespace Ui { class TimestampOverlaySettingsWidget; }

namespace nx {
namespace client {
namespace desktop {
namespace ui {

class TimestampOverlaySettingsWidget: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    TimestampOverlaySettingsWidget(QWidget* parent = nullptr);

private:
    QScopedPointer<Ui::TimestampOverlaySettingsWidget> ui;
};

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
