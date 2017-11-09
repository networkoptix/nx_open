#pragma once

namespace Ui {
class CameraInfoWidget;
}

namespace nx {
namespace client {
namespace desktop {

class CameraInfoWidget: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    CameraInfoWidget(QWidget* parent = nullptr);
    virtual ~CameraInfoWidget() override;

private:
    QScopedPointer<Ui::CameraInfoWidget> ui;
};

} // namespace desktop
} // namespace client
} // namespace nx
