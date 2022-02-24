// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "label_link_tabstop_workaround.h"

#include <QtWidgets/private/qwidgettextcontrol_p.h>

QnLabelFocusListener::QnLabelFocusListener(QObject* parent):
    QObject(parent)
{
}

bool QnLabelFocusListener::eventFilter(QObject* obj, QEvent* event)
{
    auto result = QObject::eventFilter(obj, event);
    if (event->type() == QEvent::FocusIn)
    {
        if (auto child = obj->findChild<QWidgetTextControl*>())
            child->setFocusToNextOrPreviousAnchor(true);
    }
    return result;
}
