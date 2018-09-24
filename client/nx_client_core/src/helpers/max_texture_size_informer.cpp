#include "max_texture_size_informer.h"

#include <QtCore/QByteArray>
#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLFunctions>
#include <QtQml/QQmlContext>
#include <QtQml/QQmlEngine>
#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickView>

#include <nx/utils/log/assert.h>

namespace nx::client::core {

void MaxTextureSizeInformer::obtainMaxTextureSize(
    QQuickView* view, const QByteArray& rootObjectPropertyName)
{
    NX_ASSERT(view && !rootObjectPropertyName.isEmpty());
    if (!view || rootObjectPropertyName.isEmpty())
        return;

    auto receiver = new QObject(view);

    QObject::connect(view, &QQuickWindow::beforeSynchronizing, receiver,
        [view, receiver, rootObjectPropertyName]()
        {
            QScopedPointer<QObject, QScopedPointerDeleteLater> receiverDeleter(receiver);
            const auto context = QOpenGLContext::currentContext();
            if (!context)
                return;

            GLint maxTextureSize;
            context->functions()->glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);

            auto rootObject = view->rootObject();
            if (rootObject)
            {
                rootObject->setProperty(rootObjectPropertyName, maxTextureSize);
            }
            else
            {
                QObject::connect(view, &QQuickView::statusChanged,
                    [view, rootObjectPropertyName, maxTextureSize](QQuickView::Status status)
                    {
                        if (status == QQuickView::Status::Ready)
                            view->rootObject()->setProperty(rootObjectPropertyName, maxTextureSize);
                    });
            }
        });
}

} // namespace nx::client::core
