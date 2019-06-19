#include "gl_functions.h"

#include <boost/preprocessor/stringize.hpp>

#include <QtCore/QDir>
#include <QtCore/QRegExp>

#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLFunctions>
#include <QtGui/QOffscreenSurface>
#include <QtWidgets/QOpenGLWidget>

#include <utils/common/warnings.h>
#include <nx/utils/app_info.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/thread/mutex.h>

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

    void overrideMaxTextureSize(GLint value);

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
    context.create();
    context.makeCurrent(&surface);

    const auto functions = context.functions();

    QnGlFunctions::OpenGLInfo result;
    m_info.version = (const char*)functions->glGetString(GL_VERSION);
    m_info.vendor = (const char*)context.functions()->glGetString(GL_VENDOR);
    m_info.renderer = (const char*)context.functions()->glGetString(GL_RENDERER);

    GLint maxTextureSize = -1;
    functions->glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);
    m_maxTextureSize = nx::utils::AppInfo::isLinux()
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

void StaticOpenGLInfo::overrideMaxTextureSize(GLint value)
{
    m_maxTextureSize = value;
}

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
        m_glWidget(glWidget),
        m_features(0)
    {
        const auto& info = StaticOpenGLInfo::instance().info();
        if (info.vendor.contains(lit("Tungsten Graphics"))
            && info.renderer.contains(lit("Gallium 0.1, Poulsbo on EMGD")))
		{
            m_features |= QnGlFunctions::ShadersBroken; /* Shaders are declared but don't work. */
		}

#ifdef Q_OS_MACX
        /* Intel HD 3000 driver handles textures with size > 4096 incorrectly (see bug #3141).
         * To fix that we have to override maximum texture size to 4096 for this graphics adapter. */
        if (info.vendor.contains(lit("Intel"))
            && QRegExp(QLatin1String(".*Intel.+3000.*")).exactMatch(info.renderer))
        {
            StaticOpenGLInfo::instance().overrideMaxTextureSize(4096);
        }
#endif


#ifdef Q_OS_LINUX
        QDir atiProcDir(lit("/proc/ati"));
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
    QString getGLString(GLenum id)
	{
        const auto functions = m_glWidget->context()->functions();
        return QLatin1String(reinterpret_cast<const char *>(functions->glGetString(id)));
	}

    QOpenGLWidget* const m_glWidget;
    QnGlFunctions::Features m_features;
};

typedef QnGlContextData<QnGlFunctionsPrivate, QnGlContextDataForwardingFactory<QnGlFunctionsPrivate> > QnGlFunctionsPrivateStorage;
Q_GLOBAL_STATIC(QnGlFunctionsPrivateStorage, qn_glFunctionsPrivateStorage);


// -------------------------------------------------------------------------------------------------

QnGlFunctions::PrivatePtr QnGlFunctions::createPrivate(QOpenGLWidget* glWidget)
{
    const auto storage = qn_glFunctionsPrivateStorage();
    PrivatePtr result;
    if(storage)
        result = storage->get(glWidget);

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

const QnGlFunctions::OpenGLInfo& QnGlFunctions::openGLInfo() const
{
    return StaticOpenGLInfo::instance().info();
}

QnGlFunctions::OpenGLInfo QnGlFunctions::openGLCachedInfo()
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

QnGlFunctions::Features QnGlFunctions::estimatedFeatures() {
#ifdef Q_OS_WIN
    return *qn_estimatedFeatures();
#else
    return QnGlFunctions::Features(0);
#endif
}

GLint QnGlFunctions::estimatedInteger(GLenum target)
{
    if(target != GL_MAX_TEXTURE_SIZE) {
        qnWarning("Target '%1' is not supported.", target);
        return 0;
    }

    return StaticOpenGLInfo::instance().maxTextureSize();
}

