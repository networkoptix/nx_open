#include "progress_bar.h"

#include <ui/style/noptix_style.h>

QnProgressBar::QnProgressBar(QWidget *parent /*= 0*/):
    base_type(parent)
{

}

void QnProgressBar::initStyleOption(QnStyleOptionProgressBar *option) const {
    base_type::initStyleOption(option);
    option->separators << m_separators;
}

void QnProgressBar::paintEvent(QPaintEvent *event) {
    QStylePainter paint(this);
    QnStyleOptionProgressBar opt;
    initStyleOption(&opt);
    paint.drawControl(QStyle::CE_ProgressBar, opt);
}

QList<int> QnProgressBar::separators() const {
    return m_separators;
}

void QnProgressBar::setSeparators(const QList<int> &separators) {
    m_separators = separators;
}
