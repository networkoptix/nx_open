// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "text_input_workaround.h"

#include <QtQuick/QQuickItem>

#include <nx/build_info.h>

namespace nx::vms::client::mobile {

namespace {

class TextScrollWorkaroundFilter: public QObject
{
    virtual bool eventFilter(QObject* targetObject, QEvent* event) override
    {
        if (event->type() != QEvent::InputMethodQuery)
            return QObject::eventFilter(targetObject, event);

        QInputMethodQueryEvent* const queryEvent = static_cast<QInputMethodQueryEvent*>(event);
        if (queryEvent->queries() != Qt::InputMethodQuery::ImCursorRectangle)
            return QObject::eventFilter(targetObject, event);

        queryEvent->setValue(Qt::InputMethodQuery::ImCursorRectangle, {});
        return true;
    }
};

} // namespace

void TextInputWorkaround::registerQmlType()
{
    static TextInputWorkaround sWorkaround;
    qmlRegisterSingletonInstance<TextInputWorkaround>(
        "nx.vms.client.mobile", 1, 0, "TextInputWorkaround", &sWorkaround);
}

void TextInputWorkaround::setup(QQuickItem* item) const
{
    if (!nx::build_info::isIos())
        return;

    static TextScrollWorkaroundFilter sFilter;
    item->installEventFilter(&sFilter);
}

} // namespace nx::vms::client::mobile
