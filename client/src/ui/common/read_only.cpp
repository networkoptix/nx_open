#include "read_only.h"

#include <QtWidgets/QWidget>
#include <QtGui/QKeyEvent>
#include <QtCore/QMetaProperty>

#include <utils/common/warnings.h>
#include <utils/common/event_processors.h>

#include "read_only_event_eater.h"


namespace {
    QnReadOnlyEventEater *readOnlyEventEater(QWidget *widget) {
        static const char *propertyName = "_qn_readOnlyEventEater";

        QnReadOnlyEventEater *result = widget->property(propertyName).value<QnReadOnlyEventEater*>();
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
