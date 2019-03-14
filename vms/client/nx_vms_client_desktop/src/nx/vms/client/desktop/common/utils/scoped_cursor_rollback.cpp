#include "scoped_cursor_rollback.h"

#include <QtWidgets/QWidget>

namespace nx::vms::client::desktop {

ScopedCursorRollback::ScopedCursorRollback(QWidget* widget):
    m_widget(widget),
    m_hadCursor(widget ? widget->testAttribute(Qt::WA_SetCursor) : false),
    m_oldCursor(m_hadCursor ? widget->cursor() : QCursor())
{
}

ScopedCursorRollback::ScopedCursorRollback(QWidget* widget, const QCursor& cursor):
    ScopedCursorRollback(widget)
{
    if (widget)
        widget->setCursor(cursor);
}

ScopedCursorRollback::~ScopedCursorRollback()
{
    if (!m_widget)
        return;

    if (m_hadCursor)
        m_widget->setCursor(m_oldCursor);
    else
        m_widget->unsetCursor();
}

} // namespace nx::vms::client::desktop
