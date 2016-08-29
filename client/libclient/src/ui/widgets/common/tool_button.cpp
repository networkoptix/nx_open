#include "tool_button.h"

QnToolButton::QnToolButton(QWidget* parent)
    : base_type(parent)
{
}

void QnToolButton::adjustIconSize()
{
    QIcon icon = this->icon();
    if (icon.isNull())
        return;

    setIconSize(icon.actualSize(QSize(1024, 1024)));
}

void QnToolButton::mousePressEvent(QMouseEvent* event)
{
    emit justPressed();
    base_type::mousePressEvent(event);
}

void QnToolButton::showEvent(QShowEvent *event)
{
    /*
    auto window = windowHandle();
    const auto flags = (window->flags() & Qt::WindowType_Mask);
    window->setFlags(flags | Qt::ForeignWindow);
    */
}
