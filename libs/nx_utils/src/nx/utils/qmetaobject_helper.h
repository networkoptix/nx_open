#include <QtCore/QMetaObject>

namespace nx::utils {

/**
 * Emit QT signal asynchronously via sender object event queue.
 */
template <typename ... Args>
void emitAsync(QObject* object, const char* signal, const Args&... args)
{
    QMetaObject::invokeMethod(object, signal, Qt::QueuedConnection,
        QArgument(typeid(args).name(), args)...);
}

template <typename Object, typename Func, typename ... Args>
void emitAsync(Object* object, Func func, const Args&... args)
{
    QMetaObject::invokeMethod(object, std::bind(func, object, args...), Qt::QueuedConnection);
}

} // namespace nx::utils
