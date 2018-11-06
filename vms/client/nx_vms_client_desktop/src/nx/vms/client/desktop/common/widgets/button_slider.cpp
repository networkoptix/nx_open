#include <QtWidgets/QVBoxLayout>
#include <QtGui/QMouseEvent>

#include "button_slider.h"
#include "hover_button.h"

#include <ui/style/globals.h>
#include <ui/style/skin.h>
#include <ui/style/nx_style.h>
#include <ui/style/helper.h>

namespace {
    const QString kIconPlus(lit("text_buttons/arythmetic_plus.png"));
    const QString kIconPlusHovered(lit("text_buttons/arythmetic_plus_hovered.png"));
    const QString kIconMinus(lit("text_buttons/arythmetic_minus.png"));
    const QString kIconMinusHovered(lit("text_buttons/arythmetic_minus_hovered.png"));
    const QSize kButtonSize(20, 20);
    const int kSliderHeight = 120;
    const int kControlBtn = Qt::LeftButton;
} // namespace

namespace nx::vms::client::desktop {

VButtonSlider::VButtonSlider(QWidget* parent):
    QWidget(parent)
{
    const auto container = new QVBoxLayout(this);
    m_add = new HoverButton(kIconPlus, kIconPlusHovered, this);
    m_add->setMinimumSize(kButtonSize);
    m_slider = new QSlider(this);
    m_slider->setOrientation(Qt::Orientation::Vertical);
    m_slider->setMinimumHeight(kSliderHeight);
    // We do not want this style right now.
    //m_slider->setProperty(style::Properties::kSliderFeatures,
    //    static_cast<int>(style::SliderFeature::FillingUp));
    m_dec = new HoverButton(kIconMinus, kIconMinusHovered, this);
    m_dec->setMinimumSize(kButtonSize);
    m_label = new QLabel();
    container->setContentsMargins(8, 8, 8, 8);
    container->setSpacing(8);
    container->addWidget(m_add);
    container->setAlignment(m_add, Qt::AlignTop | Qt::AlignHCenter);
    container->addWidget(m_slider);
    container->setAlignment(m_slider, Qt::AlignTop | Qt::AlignHCenter);
    container->addWidget(m_dec);
    container->setAlignment(m_dec, Qt::AlignTop | Qt::AlignHCenter);
    container->addWidget(m_label);
    container->setAlignment(m_label, Qt::AlignTop | Qt::AlignHCenter);
    setLayout(container);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    connect(m_add, &QAbstractButton::pressed, this, &VButtonSlider::onIncrementOn);
    connect(m_add, &QAbstractButton::released, this, &VButtonSlider::onIncrementOff);
    connect(m_dec, &QAbstractButton::pressed, this, &VButtonSlider::onDecrementOn);
    connect(m_dec, &QAbstractButton::released, this, &VButtonSlider::onDecrementOff);
    connect(m_slider, &QAbstractSlider::valueChanged, this, &VButtonSlider::onSliderChanged);
    connect(m_slider, &QAbstractSlider::sliderReleased, this, &VButtonSlider::onSliderReleased);
}

void VButtonSlider::setText(const QString& text)
{
    m_label->setText(text);
}

void VButtonSlider::setButtonIncrement(int increment)
{
    m_increment = increment;
}

void VButtonSlider::onIncrementOn()
{
    int value = m_slider->sliderPosition();
    m_slider->setSliderPosition(value + m_increment);
}

void VButtonSlider::onIncrementOff()
{
    m_slider->setSliderPosition(0);
}

void VButtonSlider::onDecrementOn()
{
    int value = m_slider->sliderPosition();
    m_slider->setSliderPosition(value - m_increment);
}

void VButtonSlider::onDecrementOff()
{
    m_slider->setSliderPosition(0);
}

void VButtonSlider::onSliderChanged(int value)
{
    emit valueChanged(value);
}

void VButtonSlider::onSliderReleased()
{
    // Zoom/focus should reset to zero when handler is released.
    m_slider->setSliderPosition(0);
    emit valueChanged(0);
}

void VButtonSlider::setMinimum(int value)
{
    m_slider->setMinimum(value);
}

int VButtonSlider::minimum() const
{
    return m_slider->minimum();
}

void VButtonSlider::setMaximum(int value)
{
    m_slider->setMaximum(value);
}

int VButtonSlider::maximum() const
{
    return m_slider->maximum();
}

void VButtonSlider::setSliderPosition(int value)
{
    m_slider->setSliderPosition(value);
}

int VButtonSlider::sliderPosition() const
{
    return m_slider->sliderPosition();
}

} // namespace nx::vms::client::desktop
