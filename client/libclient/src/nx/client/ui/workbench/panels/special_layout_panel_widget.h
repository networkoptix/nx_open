#pragma once

#include <QtWidgets/QAbstractButton>
#include <QtWidgets/QGraphicsProxyWidget>

class QLabel;
class QHBoxLayout;

namespace nx {
namespace client {
namespace desktop {
namespace ui {
namespace workbench {

class SpecialLayoutPanelWidget: public QGraphicsProxyWidget
{
    Q_OBJECT
    using base_type = QGraphicsProxyWidget;

public:
    SpecialLayoutPanelWidget();

    void setCaption(const QString& caption);

    void addButton(QAbstractButton* button);

private:
    QLabel* m_captionLabel = nullptr;
    QHBoxLayout* m_layout = nullptr;
};

} // namespace workbench
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
