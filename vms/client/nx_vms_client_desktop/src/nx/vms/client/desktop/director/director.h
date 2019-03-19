#pragma once

#include <QtCore/QScopedPointer>

class QnWorkbenchContext;

namespace nx::vmx::client::desktop {

class Director
{
public:
    /** Returns pointer to pre-created singleton Director. */
    Director* instance();

    /** Closes client application. */
    void quit();

    /** Creates singleton Director object (called only from application.cpp). */
    static void createInstance(QnWorkbenchContext* context);

private:
    explicit Director(QnWorkbenchContext* context);

    static QScopedPointer<Director> m_director;

    QnWorkbenchContext* m_context = nullptr;
};

} // namespace nx::vmx::client::desktop
