#include "tool_tip.h"
#include <QCoreApplication>
#include <QGraphicsView>
#include <QWidget>
#include <QLabel>
#include <utils/common/checked_cast.h>

template<bool (QnToolTip::*method)(QObject *, QEvent *)>
class QnToolTipEventFilter: public QObject {
public:
    QnToolTipEventFilter(QnToolTip *toolTip, QObject *parent = NULL):
        QObject(parent),
        m_toolTip(toolTip)
    {}

    virtual bool eventFilter(QObject *watched, QEvent *event) override {
        return (m_toolTip->*method)(watched, event);
    }

private:
    QnToolTip *m_toolTip;
};

Q_GLOBAL_STATIC(QnToolTip, qn_toolTip);

QnToolTip::QnToolTip():
    m_application(qApp),
    m_toolTipEventFilter(new QnToolTipEventFilter<&QnToolTip::toolTipEventFilter>(this, this)),
    m_widgetEventFilter(new QnToolTipEventFilter<&QnToolTip::widgetEventFilter>(this, this))
{
    m_application->installEventFilter(this);
}

QnToolTip::~QnToolTip() {
    return;
}

QnToolTip *QnToolTip::instance() {
    return qn_toolTip();
}

bool QnToolTip::eventFilter(QObject *watched, QEvent *event) {
    if(event->type() == QEvent::Create)
        if(QWidget *widget = qobject_cast<QWidget *>(watched))
            widget->installEventFilter(m_widgetEventFilter);

    return false;
}

bool QnToolTip::widgetEventFilter(QObject *watched, QEvent *event) {
    if(event->type() == QEvent::Polish) {
        if(QWidget *widget = qobject_cast<QWidget *>(watched)) {
            if(widget->inherits("QTipLabel"))
                registerToolTip(checked_cast<QLabel *>(widget));

            if(QGraphicsView *view = qobject_cast<QGraphicsView *>(widget))
                registerGraphicsView(view);
        }

        watched->removeEventFilter(m_widgetEventFilter);
    }

    return false;
}

bool QnToolTip::toolTipEventFilter(QObject *watched, QEvent *event) {
    QLabel *label = static_cast<QLabel *>(watched);
        
    switch(event->type()) {
    case QEvent::Destroy:
        unregisterToolTip(label);
        break;
    case QEvent::Show:
        /* Simply kill all tooltips for now. */
        label->hide();
        label->deleteLater();
        break;
    }

    return false;
}

void QnToolTip::registerToolTip(QLabel *label) {
    label->installEventFilter(m_toolTipEventFilter);
    m_toolTips.insert(label);
}

void QnToolTip::unregisterToolTip(QLabel *label) {
    label->removeEventFilter(m_toolTipEventFilter);
    m_toolTips.remove(label);
}

void QnToolTip::registerGraphicsView(QGraphicsView *view) {
    m_graphicsViews.insert(view);

    connect(view, SIGNAL(destroyed(QObject *)), this, SLOT(at_graphicsView_destroyed(QObject *)));
}

void QnToolTip::unregisterGraphicsView(QGraphicsView *view) {
    disconnect(view, NULL, this, NULL);

    m_graphicsViews.remove(view);
}

void QnToolTip::at_graphicsView_destroyed(QObject *object) {
    unregisterGraphicsView(static_cast<QGraphicsView *>(object));
}
