#include "read_only.h"

#include <QtWidgets/QWidget>
#include <QtGui/QKeyEvent>
#include <QtCore/QMetaProperty>

#include <utils/common/warnings.h>
#include <utils/common/event_processors.h>

// -------------------------------------------------------------------------- //
// QnReadOnlyEventEater
// -------------------------------------------------------------------------- //
class QnReadOnlyEventEater: public QnMultiEventEater {
public:
    QnReadOnlyEventEater(QObject *parent = NULL);

    static QnReadOnlyEventEater *instance(QWidget *widget);

protected:
    virtual bool activate(QObject *, QEvent *event) override;
};

QnReadOnlyEventEater::QnReadOnlyEventEater(QObject *parent): QnMultiEventEater(parent) {
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

    addEventType(QEvent::HoverEnter);
    addEventType(QEvent::HoverLeave);
    addEventType(QEvent::HoverMove);

    addEventType(QEvent::Enter);
    addEventType(QEvent::Leave);
    addEventType(QEvent::FocusIn);
}

bool QnReadOnlyEventEater::activate(QObject *, QEvent *event) {
    if(event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease) {
        QKeyEvent *e = static_cast<QKeyEvent *>(event);
        if(e->key() == Qt::Key_Tab || e->key() == Qt::Key_Backtab)
            return false; /* We want the tab-focus to work. */
    }

    return true;
}

QnReadOnlyEventEater *QnReadOnlyEventEater::instance(QWidget *widget) {
    static const char *propertyName = "_qn_readOnlyEventEater";

    QnReadOnlyEventEater *result = widget->property(propertyName).value<QnReadOnlyEventEater*>();
    if(result != NULL)
        return result;

    result = new QnReadOnlyEventEater(widget);
    widget->setProperty(propertyName, QVariant::fromValue<QnReadOnlyEventEater *>(result));
    return result;
}

Q_DECLARE_OPAQUE_POINTER(QnReadOnlyEventEater *)
Q_DECLARE_METATYPE(QnReadOnlyEventEater *);


// -------------------------------------------------------------------------- //
// Public Interface
// -------------------------------------------------------------------------- //
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

    QnReadOnlyEventEater *eventEater = QnReadOnlyEventEater::instance(widget);
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
