// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "gl_functions.h"

#include <boost/preprocessor/stringize.hpp>

#include <QtCore/QDir>
#include <QtCore/QRegularExpression>

#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLFunctions>
#include <QtGui/QOffscreenSurface>
#include <QtOpenGLWidgets/QOpenGLWidget>

#include <nx/utils/log/assert.h>
#include <nx/utils/thread/mutex.h>

#include <nx/build_info.h>

#include "gl_context_data.h"

namespace
{

enum
{
    DefaultMaxTextureSize = 1024, //< A sensible default supported by most modern GPUs.
    LinuxMaxTextureSize = 4096
};

bool warnOnInvalidCalls = false;

//--------------------------------------------------------------------------------------------------

class StaticOpenGLInfo
{
public:
    static StaticOpenGLInfo& instance();

    const QnGlFunctions::OpenGLInfo& info() const;
    GLint maxTextureSize() const;

#ifdef Q_OS_MACX
    void overrideMaxTextureSize(GLint value);
#endif // Q_OS_MACX

private:
    StaticOpenGLInfo();

private:
    QnGlFunctions::OpenGLInfo m_info;
    GLint m_maxTextureSize = DefaultMaxTextureSize;
};

StaticOpenGLInfo& StaticOpenGLInfo::instance()
{
    static StaticOpenGLInfo result;
    return result;
}

StaticOpenGLInfo::StaticOpenGLInfo()
{
    QOffscreenSurface surface;
    surface.create();

    QOpenGLContext context;
    NX_CRITICAL(context.create());
    NX_CRITICAL(context.makeCurrent(&surface));

    const auto functions = context.functions();

    const auto glString =
        [functions](GLenum id) -> QString { return (const char*) functions->glGetString(id); };

    QnGlFunctions::OpenGLInfo result;
    m_info.version = glString(GL_VERSION);
    m_info.vendor = glString(GL_VENDOR);
    m_info.renderer = glString(GL_RENDERER);
    m_info.extensions = glString(GL_EXTENSIONS);

    GLint maxTextureSize = -1;
    functions->glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);
    m_maxTextureSize = nx::build_info::isLinux()
        ? std::min<GLint>(maxTextureSize, LinuxMaxTextureSize)
        : maxTextureSize;
}

const QnGlFunctions::OpenGLInfo& StaticOpenGLInfo::info() const
{
    return m_info;
}

GLint StaticOpenGLInfo::maxTextureSize() const
{
    return m_maxTextureSize;
}

#if defined(Q_OS_MACX)
void StaticOpenGLInfo::overrideMaxTextureSize(GLint value)
{
    m_maxTextureSize = value;
}
#endif // defined(Q_OS_MACX)

} // namespace

// -------------------------------------------------------------------------- //
// QnGlFunctionsPrivate
// -------------------------------------------------------------------------- //
/**
 * A per-context object that contains OpenGL functions state.
 */
class QnGlFunctionsPrivate
{
public:
    QnGlFunctionsPrivate(QOpenGLWidget* glWidget):
        m_glWidget(glWidget)
    {
        const auto& info = StaticOpenGLInfo::instance().info();
        if (info.vendor.contains("Tungsten Graphics")
            && info.renderer.contains("Gallium 0.1, Poulsbo on EMGD"))
        {
            m_features |= QnGlFunctions::ShadersBroken; /* Shaders are declared but don't work. */
        }

#ifdef Q_OS_MACX
        /* Intel HD 3000 driver handles textures with size > 4096 incorrectly (see bug #3141).
         * To fix that we have to override maximum texture size to 4096 for this graphics adapter. */
        if (info.vendor.contains("Intel")
            && QRegularExpression(QRegularExpression::anchoredPattern(QLatin1String(".*Intel.+3000.*"))).match(info.renderer).hasMatch())
        {
            StaticOpenGLInfo::instance().overrideMaxTextureSize(4096);
        }
#endif


#ifdef Q_OS_LINUX
        QDir atiProcDir("/proc/ati");
        if(atiProcDir.exists()) /* Checking for fglrx driver. */
            m_features |= QnGlFunctions::NoOpenGLFullScreen;
#endif
    }

    virtual ~QnGlFunctionsPrivate()
    {
    }

    QnGlFunctions::Features features() const {
        return m_features;
    }

    QOpenGLWidget* glWidget() const
    {
        return m_glWidget;
    }

private:
    QOpenGLWidget* const m_glWidget;
    QnGlFunctions::Features m_features;
};

typedef QnGlContextData<QnGlFunctionsPrivate, QnGlContextDataForwardingFactory<QnGlFunctionsPrivate> > QnGlFunctionsPrivateStorage;
Q_GLOBAL_STATIC(QnGlFunctionsPrivateStorage, qn_glFunctionsPrivateStorage);


// -------------------------------------------------------------------------------------------------

QnGlFunctions::PrivatePtr QnGlFunctions::createPrivate(QOpenGLWidget* glWidget)
{
    PrivatePtr result;

    if (glWidget)
    {
        const auto storage = qn_glFunctionsPrivateStorage();

        if(storage)
            result = storage->get(glWidget);
    }

    if(result.isNull()) //< Application is being shut down.
        result = PrivatePtr(new QnGlFunctionsPrivate(nullptr));

    return result;
}

QnGlFunctions::QnGlFunctions(QOpenGLWidget* glWidget):
    d(createPrivate(glWidget))
{
}

QnGlFunctions::~QnGlFunctions()
{
}

QnGlFunctions::Features QnGlFunctions::features() const
{
    return d->features();
}

QnGlFunctions::OpenGLInfo QnGlFunctions::openGLInfo()
{
    return StaticOpenGLInfo::instance().info();
}

void QnGlFunctions::setWarningsEnabled(bool enable)
{
    warnOnInvalidCalls = enable;
}

bool QnGlFunctions::isWarningsEnabled()
{
    return warnOnInvalidCalls;
}

QOpenGLWidget* QnGlFunctions::glWidget() const
{
    return d->glWidget();
}

#ifdef Q_OS_WIN
namespace {
    QnGlFunctions::Features estimateFeatures() {
        QnGlFunctions::Features result(0);

        DISPLAY_DEVICEW dd;
        dd.cb = sizeof(dd);
        DWORD dev = 0;
        while (EnumDisplayDevices(0, dev, &dd, 0)) {
            if (dd.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE) {
                QString v = QString::fromWCharArray(dd.DeviceString);
                if (v.contains(QLatin1String("Gallium 0.1, Poulsbo on EMGD")))
                    result |= QnGlFunctions::ShadersBroken;
                break;
            }
            ZeroMemory(&dd, sizeof(dd));
            dd.cb = sizeof(dd);
            dev++;
        }
        return result;
    }
    Q_GLOBAL_STATIC_WITH_ARGS(QnGlFunctions::Features, qn_estimatedFeatures, (estimateFeatures()));
}
#endif

QnGlFunctions::Features QnGlFunctions::estimatedFeatures()
{
    #if defined(Q_OS_WIN)
        return *qn_estimatedFeatures();
    #else
        return {};
    #endif
}

GLint QnGlFunctions::estimatedInteger(GLenum target)
{
    if(target != GL_MAX_TEXTURE_SIZE) {
        NX_ASSERT(false, "Target '%1' is not supported.", target);
        return 0;
    }

    return StaticOpenGLInfo::instance().maxTextureSize();
}
