#include "tool_tip.h"
#include <QCoreApplication>
#include <QWidget>

Q_GLOBAL_STATIC_WITH_INITIALIZER(QnToolTip, qn_toolTip, {
    qApp->installEventFilter(x.data());
});

QnToolTip *QnToolTip::instance() {
    return qn_toolTip();
}

bool QnToolTip::eventFilter(QObject *watched, QEvent *event) {
    switch(event->type()) {
    case QEvent::Create:
        if(QWidget *widget = qobject_cast<QWidget *>(watched))
            if(widget->windowFlags() & Qt::ToolTip)
                widget->overrideWindowFlags(widget->windowFlags() & ~Qt::BypassGraphicsProxyWidget);
        break;
    default:
        break;
    }

    return false;
}

