#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

namespace Ui { class RapidReviewSettingsWidget; }

namespace nx {
namespace client {
namespace desktop {
namespace ui {

class RapidReviewSettingsWidget: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    RapidReviewSettingsWidget(QWidget* parent = nullptr);

private:
    QScopedPointer<Ui::RapidReviewSettingsWidget> ui;
};

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
