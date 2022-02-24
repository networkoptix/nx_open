// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <QtCore/QMetaObject>

namespace nx::utils {

/**
 * Emit QT signal asynchronously via sender object event queue.
 * Warning: object should be destroyed either via 'deleteLater' call
 * or sender thread should be stopped before object is deleted.
 */
template <typename Object, typename Func, typename ... Args>
void emitAsync(Object* object, Func func, const Args&... args)
{
    QMetaObject::invokeMethod(object, std::bind(func, object, args...), Qt::QueuedConnection);
}

} // namespace nx::utils
