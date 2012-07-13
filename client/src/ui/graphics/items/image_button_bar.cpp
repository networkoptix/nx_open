#include "image_button_bar.h"

#include <QtGui/QGraphicsLinearLayout>

#include <utils/common/warnings.h>

#include "image_button_widget.h"


QnImageButtonBar::QnImageButtonBar(QGraphicsItem *parent, Qt::WindowFlags windowFlags):
    base_type(parent, windowFlags),
    m_buttonsVisibility(0)
{
    m_layout = new QGraphicsLinearLayout(Qt::Horizontal);
    m_layout->setContentsMargins(0.0, 0.0, 0.0, 0.0);
    m_layout->setSpacing(0.0);
    setLayout(m_layout);
}

QnImageButtonBar::~QnImageButtonBar() {
    return;
}

void QnImageButtonBar::addButton(int mask, QnImageButtonWidget *button) {
    if(button == NULL) {
        qnNullWarning(button);
        return;
    }

    button->setParent(this); /* We're expected to take ownership, even if button wasn't actually added. */

    if(!qIsPower2(mask))
        qnWarning("Given mask '%1' is not a power of 2.", mask);

    if(m_maskByButton.contains(button)) {
        qnWarning("Given button is already added to this button bar.");
        return;
    }

    if(m_buttonByMask.contains(mask)) {
        qnWarning("Given mask '%1' is already taken in this image button bar.", mask);
        return;
    }

    m_maskByButton[button] = mask;
    m_buttonByMask[mask] = button;
}

void QnImageButtonBar::removeButton(QnImageButtonWidget *button) {
    if(!m_maskByButton.contains(button))
        return;

    int mask = m_maskByButton.value(button);
    m_maskByButton.remove(button);
    m_buttonByMask.remove(mask);
}

QnImageButtonWidget *QnImageButtonBar::button(int mask) const {
    return m_buttonByMask.value(mask);
}

int QnImageButtonBar::buttonsVisibility() const {
    return m_buttonsVisibility;
}

void QnImageButtonBar::setButtonsVisibility(int mask) {
    if(m_buttonsVisibility == mask)
        return;

    m_buttonsVisibility = mask;

    emit buttonsVisibilityChanged();
}

void QnImageButtonBar::setButtonVisible(int mask, bool visible) {
    setButtonsVisibility(visible ? (m_buttonsVisibility | mask) : (m_buttonsVisibility & ~mask));
}

