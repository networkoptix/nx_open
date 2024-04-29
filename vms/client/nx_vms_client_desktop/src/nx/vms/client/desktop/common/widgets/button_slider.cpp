// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "button_slider.h"

#include <QtGui/QMouseEvent>
#include <QtWidgets/QVBoxLayout>

#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/style/style.h>

#include "hover_button.h"

namespace {

static const nx::vms::client::core::SvgIconColorer::ThemeSubstitutions kIconSubstitutions = {
    {QIcon::Normal, {.primary = "light10"}},
    {QIcon::Active, {.primary = "light11"}},
};

NX_DECLARE_COLORIZED_ICON(kMinusIcon, "20x20/Outline/minus.svg", kIconSubstitutions)
NX_DECLARE_COLORIZED_ICON(kPlusIcon, "20x20/Outline/add.svg", kIconSubstitutions)

const QSize kButtonSize(20, 20);
const int kSliderHeight = 120;
const int kControlBtn = Qt::LeftButton;

} // namespace

namespace nx::vms::client::desktop {

VButtonSlider::VButtonSlider(QWidget* parent):
    QWidget(parent)
{
    const auto container = new QVBoxLayout(this);
    m_add = new HoverButton(qnSkin->icon(kPlusIcon), this);
    m_add->setMinimumSize(kButtonSize);
    m_slider = new QSlider(this);
    m_slider->setOrientation(Qt::Orientation::Vertical);
    m_slider->setMinimumHeight(kSliderHeight);
    // We do not want this style right now.
    //m_slider->setProperty(style::Properties::kSliderFeatures,
    //    static_cast<int>(style::SliderFeature::FillingUp));
    m_dec = new HoverButton(qnSkin->icon(kMinusIcon), this);
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
