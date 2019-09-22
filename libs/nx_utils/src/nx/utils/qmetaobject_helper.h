#include <QtCore/QMetaObject>

namespace nx::utils {

/**
 * Emit QT signal asynchronously via sender object event queue.
 */
template <typename ... Args>
void emitAsync(QObject* object, const char* signal, const Args&... args)
{
    QMetaObject::invokeMethod(object, signal, Qt::QueuedConnection,
        QArgument<decltype(args)>(typeid(args).name(), args)...);
}

} // namespace nx::utils
