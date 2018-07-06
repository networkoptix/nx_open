#pragma once

#include <QtWidgets/QLabel>
#include <QtWidgets/QSlider>
#include <QtWidgets/QPushButton>

namespace nx {
namespace client {
namespace desktop {

/**
 * Vertical slider with additional buttons to increase and decrease values
 * TODO: Implement proper events for changing value
 * TODO: Implement min/max limits for this control
 * TODO: Complete the style. Draw additional line between the handler and center of the control
 */
class VButtonSlider : public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    VButtonSlider(QWidget* parent);
    void setText(const QString& text);

protected:
    void onIncrement();
    void onDecrement();
private:
    QAbstractButton * m_add = nullptr;
    QSlider* m_slider = nullptr;
    QAbstractButton* m_dec = nullptr;
    QLabel* m_label = nullptr;

    // Value increment for button
    int m_increment = 1;
};

} // namespace desktop
} // namespace client
} // namespace nx
