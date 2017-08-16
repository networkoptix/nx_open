#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

namespace Ui { class ExportSettingsWidget; }

class QnMediaResourceWidget;
class QnTimePeriod;

namespace nx {
namespace client {
namespace desktop {
namespace ui {

class ExportSettingsWidget: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    ExportSettingsWidget(QWidget* parent = nullptr);

private:
    QScopedPointer<Ui::ExportSettingsWidget> ui;
};

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
