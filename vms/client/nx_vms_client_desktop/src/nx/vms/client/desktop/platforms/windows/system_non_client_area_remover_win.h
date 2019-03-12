#pragma once

#if !defined(Q_OS_WIN)
#error This module is Windows-only.
#endif

#include <QtCore/QScopedPointer>

class QWidget;

namespace nx::vms::client::desktop {

/**
 * An utility to remove system frames from top-level windows of specified widgets and optionally
 * to delegate non-client hit testing to EmulatedNonClientArea instances, if found in the widgets.
 */
class SystemNonClientAreaRemover final
{
    SystemNonClientAreaRemover();
    ~SystemNonClientAreaRemover();

public:
    // This method may be called for the first time only when global QApplication object exists.
    static SystemNonClientAreaRemover& instance();

    void apply(QWidget* widget);
    bool restore(QWidget* widget);

private:
    class Private;
    const QScopedPointer<Private> d;
};

} // namespace nx::vms::client::desktop
