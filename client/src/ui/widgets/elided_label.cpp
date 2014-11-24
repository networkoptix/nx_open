#include "elided_label.h"

#include <QtWidgets/5.2.1/QtWidgets/private/qlabel_p.h>
#include <QtWidgets/QStyleOption>

QnElidedLabel::QnElidedLabel(QWidget *parent) :
    QLabel(parent),
    m_elideMode(Qt::ElideLeft)
{
}

Qt::TextElideMode QnElidedLabel::elideMode() const {
    return m_elideMode;
}

void QnElidedLabel::setElideMode(Qt::TextElideMode elideMode) {
    m_elideMode = elideMode;
    update();
}

void QnElidedLabel::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event)

    QLabelPrivate *d = static_cast<QLabelPrivate*>(d_ptr.data());

    QStyle *style = QWidget::style();
    QPainter painter(this);
    drawFrame(&painter);
    QRect cr = contentsRect();
    cr.adjust(d->margin, d->margin, -d->margin, -d->margin);
    int align = QStyle::visualAlignment(d->isTextLabel ? (d->text.isRightToLeft() ? Qt::RightToLeft : Qt::LeftToRight)
                                                       : layoutDirection(), QFlag(d->align));
    int m = d->indent;
    if (m < 0 && frameWidth()) // no indent, but we do have a frame
        m = fontMetrics().width(QLatin1Char('x')) / 2 - d->margin;
    if (m > 0) {
        if (align & Qt::AlignLeft)
            cr.setLeft(cr.left() + m);
        if (align & Qt::AlignRight)
            cr.setRight(cr.right() - m);
        if (align & Qt::AlignTop)
            cr.setTop(cr.top() + m);
        if (align & Qt::AlignBottom)
            cr.setBottom(cr.bottom() - m);
    }

    if (d->isTextLabel) {
        QStyleOption opt;
        opt.initFrom(this);
        int flags = align | (d->text.isRightToLeft() ? Qt::TextForceRightToLeft
                                                     : Qt::TextForceLeftToRight);
        QString actualText = d->text;
        if (m_elideMode != Qt::ElideNone)
            actualText = fontMetrics().elidedText(d->text, m_elideMode, cr.width(), flags);

        style->drawItemText(&painter, cr, flags, opt.palette, isEnabled(), actualText, foregroundRole());
    }
}

