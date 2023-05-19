// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "opengl_context_logger.h"

#include <QtCore/QPointer>
#include <QtCore/QSet>
#include <QtGui/QGuiApplication>
#include <QtGui/QOpenGLContext>
#include <QtGui/QScreen>
#include <QtQuick/QQuickWindow>
#include <QtQuickWidgets/QQuickWidget>

#ifdef _WIN32
#include <QtPlatformHeaders/QWGLNativeContext>
#endif

#include <nx/utils/log/log.h>
#include <nx/utils/range_adapters.h>

namespace nx::vms::client::desktop {

class OpenGLContextLogger::Private: public QObject
{
    OpenGLContextLogger* const q;

    struct WindowData
    {
        QPointer<QQuickWidget> widget;
        QPointer<QOpenGLContext> context;
    };

    struct ContextData
    {
        void* window = nullptr;
        QVariant handle;
    };

public:
    Private(OpenGLContextLogger* q): q(q)
    {
        QGuiApplication::instance()->installEventFilter(this);
    }

    virtual ~Private() override
    {
        QStringList windowInfo;
        for (const auto& [window, data]: nx::utils::constKeyValueRange(m_windows))
            windowInfo.push_back(NX_FMT("\t%1", toString(window)));

        if (!windowInfo.empty())
            NX_INFO(q, "Remaining QtQuick windows at destruction:\n%1", windowInfo.join("\n"));

        QStringList contextInfo;
        for (const auto& [context, data]: nx::utils::constKeyValueRange(m_contexts))
        {
            contextInfo.push_back(NX_FMT("\t%1, handle: %2",
                toHex(context), nativeHandleToString(data.handle)));
        }

        if (!contextInfo.empty())
            NX_INFO(q, "Remaining OpenGL contexts at destruction:\n%1", contextInfo.join("\n"));
    }

    virtual bool eventFilter(QObject* object, QEvent* event) override
    {
        if (const auto window = qobject_cast<QQuickWindow*>(object))
            registerWindow(window);

        if (const auto widget = qobject_cast<QQuickWidget*>(object))
            registerWindow(widget->quickWindow(), widget);

        return false;
    }

private:
    void registerWindow(QQuickWindow* window, QQuickWidget* widget = nullptr)
    {
        if (!window)
            return;

        if (m_windows.contains(window))
        {
            updateWindowData(window, widget);
            return;
        }

        connect(window, &QObject::destroyed, this, &Private::handleWindowDestroyed);

        connect(window, &QObject::objectNameChanged, this,
            [this, window]() { handleWindowUpdated(window); });

        connect(window, &QQuickWindow::sceneGraphError,
            this, &Private::handleSceneGraphError);

        if (widget)
        {
            connect(widget, &QObject::objectNameChanged, this,
                [this, widget]() { handleWidgetUpdated(widget); });
        }

        connect(window, &QQuickWindow::openglContextCreated, this,
            [this, window](QOpenGLContext* context) { handleContextCreated(window, context); });

        connect(window, &QQuickWindow::sceneGraphInitialized, this,
            [this, window]()
            {
                NX_INFO(q, "Scene graph initialized for %1", toString(window));
                updateWindowData(window, window->openglContext());
            });

        connect(window, &QQuickWindow::sceneGraphInvalidated, this,
            [this, window]()
            {
                NX_INFO(q, "Scene graph invalidated for %1", toString(window));
                updateWindowData(window, window->openglContext());
            });

        m_windows.insert(window, {.widget = widget});

        NX_INFO(q, "New QtQuick window detected: %1, total %2 windows",
            toString(window), m_windows.size());

        if (auto context = window->openglContext())
            handleContextCreated(window, context);
    }

    void updateWindowData(QQuickWindow* window, QQuickWidget* widget)
    {
        if (!widget || m_windows.value(window).widget == widget)
            return;

        if (widget)
            m_windows[window].widget = widget;

        connect(widget, &QObject::objectNameChanged, this,
            [this, widget]() { handleWidgetUpdated(widget); });

        handleWindowUpdated(window);
    }

    void updateWindowData(QQuickWindow* window, QOpenGLContext* context)
    {
        if (!NX_ASSERT(m_windows.contains(window)))
            return;

        if (m_windows.value(window).context == context)
            return;

        if (context)
        {
            NX_INFO(q, "New OpenGL context %1 created for window %2",
                toHex(context), toHex(window));

            m_contexts.insert(context, {.window = window, .handle = context->nativeHandle()});

            connect(context, &QObject::destroyed, this, &Private::handleContextDestroyed);

            connect(context, &QOpenGLContext::aboutToBeDestroyed, this,
                [this, context]() { handleContextAboutToBeDestroyed(context); });
        }

        m_windows[window].context = context;
        handleWindowUpdated(window);
    }

    void handleSceneGraphError(QQuickWindow::SceneGraphError error, const QString& message)
    {
        if (const auto window = qobject_cast<QQuickWindow*>(sender()))
        {
            NX_ERROR(q, "Scene graph error %1 for window %2, message: %3",
                error, toString(window), message);
        }
        else if (const auto widget = qobject_cast<QQuickWidget*>(sender()))
        {
            NX_ERROR(q, "Scene graph error %1 for widget %2, message: %3",
                error, toString(widget), message);
        }

        nx::utils::crashProgram(message);
    }

    void handleWidgetUpdated(QQuickWidget* widget)
    {
        if (auto window = widget->quickWindow())
            handleWindowUpdated(window);
    }

    void handleWindowUpdated(QQuickWindow* window)
    {
        NX_INFO(q, "QtQuick window information updated: %1", toString(window));
    }

    void handleWindowDestroyed(QObject* window)
    {
        m_windows.remove(static_cast<QQuickWindow*>(window));

        NX_INFO(q, "QtQuick window destroyed: %1, total %2 windows",
            toString(window), m_windows.size());
    }

    void handleContextCreated(QQuickWindow* window, QOpenGLContext* context)
    {
        updateWindowData(window, context);
    }

    void handleContextDestroyed(QObject* context)
    {
        NX_INFO(q, "OpenGL context %1 is destroyed (window %2)",
            toHex(context), toHex(m_contexts.value(static_cast<QOpenGLContext*>(context)).window));

        m_contexts.remove(static_cast<QOpenGLContext*>(context));
    }

    void handleContextAboutToBeDestroyed(QOpenGLContext* context)
    {
        NX_INFO(q, "OpenGL context %1 is about to be destroyed (window %2)",
            toString(context), toHex(m_contexts.value(context).window));
    }

    QString nativeHandleToString(const QVariant& handle) const
    {
#ifdef _WIN32
        const auto wglContext = handle.value<QWGLNativeContext>();
        return NX_FMT("(HGLRC: %1, HWND: %2)",
            toHex(wglContext.context()),
            toHex(wglContext.window()));
#else
        return NX_FMT("%1", handle);
#endif
    }

    QString toString(QObject* object) const
    {
        if (!object)
            return "null";

        if (const auto screen = qobject_cast<QScreen*>(object))
        {
            return NX_FMT("%1 %2 (name: '%3')",
                object->metaObject()->className(),
                toHex(object),
                screen->name());
        }

        if (const auto context = qobject_cast<QOpenGLContext*>(object))
        {
            return NX_FMT("%1 %2 (handle: %3, format: %4, surface: %5, screen: %6)",
                object->metaObject()->className(),
                toHex(object),
                nativeHandleToString(context->nativeHandle()),
                context->format(),
                context->surface(),
                toString(context->screen()));
        }

        if (const auto window = qobject_cast<QQuickWindow*>(object))
        {
            const auto data = m_windows.value(window);
            if (data.widget)
            {
                return NX_FMT("%1 %2 (name: '%3'), widget %4, context %5",
                    object->metaObject()->className(),
                    toHex(object),
                    object->objectName(),
                    toString(data.widget),
                    toString(data.context));
            }

            return NX_FMT("%1 %2 (name: '%3'), context: %4",
                object->metaObject()->className(),
                toHex(object),
                object->objectName(),
                toString(data.context));
        }

        return NX_FMT("%1 %2 (name: '%3')",
            object->metaObject()->className(),
            toHex(object),
            object->objectName());
    }

    QString toHex(void* value) const
    {
        return QString("0x") + QString::number((qlonglong) value, 16);
    }

private:
    QHash<QQuickWindow*, WindowData> m_windows;
    QHash<QOpenGLContext*, ContextData> m_contexts;
};

OpenGLContextLogger::OpenGLContextLogger(QObject* parent):
    QObject(parent),
    d(new Private{this})
{
}

OpenGLContextLogger::~OpenGLContextLogger()
{
    // Required here for forward-declared scoped pointer destruction.
}

} // namespace nx::vms::client::desktop
