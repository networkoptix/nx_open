#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

namespace Ui { class ExportLayoutSettingsWidget; }

class QnMediaResourceWidget;
class QnTimePeriod;

namespace nx {
namespace client {
namespace desktop {

class ExportLayoutSettingsWidget: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    ExportLayoutSettingsWidget(QWidget* parent = nullptr);

private:
    QScopedPointer<Ui::ExportLayoutSettingsWidget> ui;
};

} // namespace desktop
} // namespace client
} // namespace nx
