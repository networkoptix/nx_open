#include "button_slider.h"
#include "hover_button.h"

#include <QtWidgets/QStylePainter>
#include <QtWidgets/QVBoxLayout>
#include <QMouseEvent>

#include <ui/style/globals.h>
#include <ui/style/skin.h>
#include <ui/style/nx_style.h>
#include <ui/style/helper.h>

namespace {
    const QString kIconPlus(lit("buttons/arythmetic_plus.png"));
    const QString kIconPlusHovered(lit("buttons/arythmetic_plus_hovered.png"));
    const QString kIconMinus(lit("buttons/arythmetic_minus.png"));
    const QString kIconMinusHovered(lit("buttons/arythmetic_minus_hovered.png"));
    const QSize kButtonSize(20, 20);
    const int kSliderHeight = 120;
    const int kControlBtn = Qt::LeftButton;
}

namespace nx {
namespace client {
namespace desktop {

VButtonSlider::VButtonSlider(QWidget* parent)
    :QWidget(parent)
{
    QVBoxLayout* container = new QVBoxLayout(this);
    m_add = new HoverButton(kIconPlus, kIconPlusHovered, this);
    m_add->setMinimumSize(kButtonSize);
    m_slider = new QSlider(this);
    m_slider->setOrientation(Qt::Orientation::Vertical);
    m_slider->setMinimumHeight(kSliderHeight);
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

    connect(m_add, &QAbstractButton::pressed, this, &VButtonSlider::onIncrement);
    connect(m_dec, &QAbstractButton::pressed, this, &VButtonSlider::onDecrement);
}

void VButtonSlider::setText(const QString& text)
{
    m_label->setText(text);
}

void VButtonSlider::onIncrement()
{
    // TODO: #dkargin implement it
}

void VButtonSlider::onDecrement()
{
    // TODO: #dkargin implement it
}
} // namespace desktop
} // namespace client
} // namespace nx
