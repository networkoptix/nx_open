#pragma once

#include <QtWidgets/QLabel>
#include <QtWidgets/QSlider>
#include <QtWidgets/QPushButton>

namespace nx::vms::client::desktop {

/**
 * Vertical slider with additional buttons to increase and decrease values.
 * TODO: Complete the style. Draw additional line between the handler and center of the control.
 * TODO: Remove the label.
 */
class VButtonSlider: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    VButtonSlider(QWidget* parent);

    // Set label text.
    void setText(const QString& text);
    void setButtonIncrement(int increment);

    // Get current position of the slider.
    int sliderPosition() const;
    void setSliderPosition(int value);

    void setMinimum(int value);
    int minimum() const;

    void setMaximum(int value);
    int maximum() const;

signals:
    // Emitted when value is changed by a user action.
    void valueChanged(int value);

protected:
    // Internal event for increment button.
    void onIncrementOn();
    void onIncrementOff();
    // Internal event for decrement button.
    void onDecrementOn();
    void onDecrementOff();
    // Event from the slider.
    void onSliderChanged(int value);
    void onSliderReleased();

private:
    QAbstractButton* m_add = nullptr;
    QSlider* m_slider = nullptr;
    QAbstractButton* m_dec = nullptr;
    QLabel* m_label = nullptr;

    // Value increment for button.
    int m_increment = 1;
    int m_position = 0;
};

} // namespace nx::vms::client::desktop
