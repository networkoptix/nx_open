#pragma once

#include <ui/widgets/common/panel.h>

namespace Ui {
class CameraInfoWidget;
}

namespace nx {
namespace client {
namespace desktop {

class CameraInfoWidget: public QnPanel
{
    Q_OBJECT
    using base_type = QnPanel;

public:
    CameraInfoWidget(QWidget* parent = nullptr);
    virtual ~CameraInfoWidget() override;

private:
    QScopedPointer<Ui::CameraInfoWidget> ui;
};

} // namespace desktop
} // namespace client
} // namespace nx
