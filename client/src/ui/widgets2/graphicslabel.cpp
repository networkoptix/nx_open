#include "graphicslabel.h"
#include "graphicslabel_p.h"

void GraphicsLabelPrivate::init()
{
    Q_Q(GraphicsLabel);

    textItem = new QGraphicsSimpleTextItem(q);
    updateTextBrush();
    updateTextFont();

    q->setSizePolicy(QSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred,
        QSizePolicy::Label));
}

void GraphicsLabelPrivate::updateTextFont() 
{
    Q_Q(GraphicsLabel);

    textItem->setFont(q->font());
}

void GraphicsLabelPrivate::updateTextBrush() 
{
    Q_Q(GraphicsLabel);
    
    textItem->setBrush(q->palette().color(q->isEnabled() ? QPalette::Active : QPalette::Disabled, QPalette::WindowText));
}

GraphicsLabel::GraphicsLabel(QGraphicsItem *parent, Qt::WindowFlags f)
    : GraphicsFrame(*new GraphicsLabelPrivate(), parent, f)
{
    Q_D(GraphicsLabel);
    d->init();
}

GraphicsLabel::GraphicsLabel(const QString &text, QGraphicsItem *parent, Qt::WindowFlags f)
    : GraphicsFrame(*new GraphicsLabelPrivate(), parent, f)
{
    Q_D(GraphicsLabel);
    d->init();
    setText(text);
}

GraphicsLabel::~GraphicsLabel()
{
}

void GraphicsLabel::setText(const QString &text)
{
    Q_D(GraphicsLabel);
    if (d->textItem->text() == text)
        return;

    d->textItem->setText(text);

    updateGeometry();
}

QString GraphicsLabel::text() const
{
    Q_D(const GraphicsLabel);
    
    return d->textItem->text();
}

void GraphicsLabel::clear()
{
    setText(QString());
}

void GraphicsLabel::setNum(int num)
{
    QString str;
    str.setNum(num);
    setText(str);
}

void GraphicsLabel::setNum(double num)
{
    QString str;
    str.setNum(num);
    setText(str);
}

QSizeF GraphicsLabel::sizeHint(Qt::SizeHint which, const QSizeF &constraint) const {
    Q_D(const GraphicsLabel);

    if(which == Qt::MinimumSize) {
        return d->textItem->boundingRect().size();
    } else {
        return base_type::sizeHint(which, constraint);
    }
}

void GraphicsLabel::changeEvent(QEvent *event) {
    Q_D(GraphicsLabel);

    switch(event->type()) {
    case QEvent::FontChange:
        d->updateTextFont();
        break;
    case QEvent::PaletteChange:
    case QEvent::EnabledChange:
        d->updateTextBrush();
        break;
    default:
        break;
    }

    base_type::changeEvent(event);
}

