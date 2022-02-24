// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QPointer>
#include <QtGui/QCursor>

class QWidget;

namespace nx::vms::client::desktop {

class ScopedCursorRollback final
{
public:
    explicit ScopedCursorRollback(QWidget* widget);
    ScopedCursorRollback(QWidget* widget, const QCursor& cursor);

    ~ScopedCursorRollback();

private:
    const QPointer<QWidget> m_widget;
    const bool m_hadCursor = false;
    const QCursor m_oldCursor;
};

} // namespace nx::vms::client::desktop
