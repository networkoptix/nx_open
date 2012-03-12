#include "read_only.h"

#include <QtGui/QWidget>
#include <QtGui/QKeyEvent>

#include <utils/common/warnings.h>
#include <utils/common/event_processors.h>

namespace {

    class QnReadOnlyEventEater: public QnMultiEventEater {
    public:
        QnReadOnlyEventEater(QObject *parent = NULL): QnMultiEventEater(parent) {
            addEventType(QEvent::MouseButtonPress);
            addEventType(QEvent::MouseButtonRelease);
            addEventType(QEvent::MouseButtonDblClick);
            addEventType(QEvent::MouseMove);

            addEventType(QEvent::Wheel);
            
            addEventType(QEvent::NonClientAreaMouseButtonDblClick);
            addEventType(QEvent::NonClientAreaMouseButtonPress);
            addEventType(QEvent::NonClientAreaMouseButtonRelease);
            addEventType(QEvent::NonClientAreaMouseMove);

            addEventType(QEvent::DragEnter);
            addEventType(QEvent::DragMove);
            addEventType(QEvent::DragLeave);
            addEventType(QEvent::Drop);

            addEventType(QEvent::TabletMove);
            addEventType(QEvent::TabletPress);
            addEventType(QEvent::TabletRelease);

            addEventType(QEvent::TouchBegin);
            addEventType(QEvent::TouchEnd);
            addEventType(QEvent::TouchUpdate);

            addEventType(QEvent::KeyPress);
            addEventType(QEvent::KeyRelease);
        }

    protected:
        virtual bool activate(QObject *, QEvent *event) override {
            if(event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease) {
                QKeyEvent *e = static_cast<QKeyEvent *>(event);
                if(e->key() == Qt::Key_Tab || e->key() == Qt::Key_Backtab)
                    return false; /* We want the tab-focus to work. */
            }

            return true;
        }
    };

} // anonymous namespace

Q_DECLARE_METATYPE(QnReadOnlyEventEater *);

namespace {
    QnReadOnlyEventEater *readOnlyEventEater(QWidget *widget) {
        static const char *propertyName = "_qn_readOnlyEventEater";

        QnReadOnlyEventEater *result = widget->property(propertyName).value<QnReadOnlyEventEater *>();
        if(result != NULL)
            return result;

        result = new QnReadOnlyEventEater(widget);
        widget->setProperty(propertyName, QVariant::fromValue<QnReadOnlyEventEater *>(result));
        return result;
    }

} // anonymous namespace


void setReadOnly(QWidget *widget, bool readOnly) {
    if(!widget) {
        qnNullWarning(widget);
        return;
    }

    if(isReadOnly(widget) == readOnly)
        return;

    int index = widget->metaObject()->indexOfProperty("readOnly");
    if(index != -1) {
        widget->metaObject()->property(index).write(widget, readOnly);
        return;
    }

    QnReadOnlyEventEater *eventEater = readOnlyEventEater(widget);
    if(readOnly) {
        widget->installEventFilter(eventEater);
    } else {
        widget->removeEventFilter(eventEater);
    }
    widget->setProperty("readOnly", readOnly);
}

bool isReadOnly(const QWidget *widget) {
    if(!widget) {
        qnNullWarning(widget);
        return true;
    }

    return widget->property("readOnly").toBool();
}
