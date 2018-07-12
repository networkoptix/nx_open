#pragma once

#include <QtCore/QPointer>
#include <QtGui/QCursor>

class QWidget;

namespace nx {
namespace client {
namespace desktop {

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

} // namespace desktop
} // namespace client
} // namespace nx
