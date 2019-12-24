#include "texture_size_helper.h"

#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLFunctions>
#include <QtQuick/QQuickWindow>

namespace nx::vms::client::core {

static constexpr int kDefaultMaxSize = 2048;

TextureSizeHelper::TextureSizeHelper(QObject* parent):
    QObject(parent),
    m_maxTextureSize(kDefaultMaxSize)
{
}

int TextureSizeHelper::maxTextureSize() const
{
    return m_maxTextureSize.load();
}

QQuickWindow* TextureSizeHelper::window() const
{
    return m_window;
}

void TextureSizeHelper::setWindow(QQuickWindow* window)
{
    if (m_window == window)
        return;

    if (m_window)
        m_window->disconnect(this);

    m_window = window;
    emit windowChanged();

    if (m_window)
    {
        connect(m_window, &QQuickWindow::beforeSynchronizing, this,
            [this]()
            {
                const auto context = QOpenGLContext::currentContext();
                if (!context)
                    return;

                m_window->disconnect(this);

                int size = 0;
                context->functions()->glGetIntegerv(GL_MAX_TEXTURE_SIZE, &size);
                setMaxTextureSize(size);
            },
            Qt::DirectConnection);
    }
    else
    {
        setMaxTextureSize(kDefaultMaxSize);
    }
}

void TextureSizeHelper::registerQmlType()
{
    qmlRegisterType<TextureSizeHelper>("nx.vms.client.core", 1, 0, "TextureSizeHelper");
}

void TextureSizeHelper::setMaxTextureSize(int size)
{
    const int oldSize = m_maxTextureSize.exchange(size);
    if (oldSize != size)
        emit maxTextureSizeChanged();
}

} // namespace nx::vms::client::core
