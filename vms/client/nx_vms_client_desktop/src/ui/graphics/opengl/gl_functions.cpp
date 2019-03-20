#include "gl_functions.h"

#include <boost/preprocessor/stringize.hpp>

#include <QtCore/QDir>
#include <QtCore/QRegExp>

#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLFunctions>
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

} // namespace

/**
 * A global object that contains state that is shared between all OpenGL functions
 * instances.
 */
class QnGlFunctionsGlobal
{
public:
    QnGlFunctionsGlobal():
        m_initialized(false),
        m_maxTextureSize(DefaultMaxTextureSize)
    {}

    GLint maxTextureSize() const {
        return m_maxTextureSize;
    }

    void initialize(QOpenGLWidget* glWidget)
    {
        QnMutexLocker locker( &m_mutex );
        if(m_initialized)
            return;

        if(!QOpenGLContext::currentContext())
        {
            if(!glWidget)
            {
                NX_ASSERT(false, "Can't get any OpenGL context!");
                return;
            }

            glWidget->makeCurrent();
        }

        m_initialized = true;
        locker.unlock();

        GLint maxTextureSize = -1;

        const auto context = glWidget->context();
        const auto functions = context->functions();
        functions->glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);
        maxTextureSize = nx::utils::AppInfo::isLinux()
            ? std::min<GLint>(maxTextureSize, LinuxMaxTextureSize)
            : maxTextureSize;
    }

    // This function is for MacOS workaround for wrong max texture size of Intel HD3000
    void overrideMaxTextureSize(GLint maxTextureSize)
    {
        m_maxTextureSize = maxTextureSize;
    }

private:
    QnMutex m_mutex;
    bool m_initialized;
    GLint m_maxTextureSize;
};

Q_GLOBAL_STATIC(QnGlFunctionsGlobal, qn_glFunctionsGlobal)


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
        qn_glFunctionsGlobal()->initialize(glWidget);

        m_openGLInfo.version = getGLString(GL_VERSION);
        m_openGLInfo.vendor = getGLString(GL_VENDOR);
        m_openGLInfo.renderer = getGLString(GL_RENDERER);

        {
			QnMutexLocker lock(&m_mutex);
			m_openGLInfoCache = m_openGLInfo;
		}

        m_openGLInfoCache = m_openGLInfo;
        if (m_openGLInfo.vendor.contains(lit("Tungsten Graphics")) &&
			m_openGLInfo.renderer.contains(lit("Gallium 0.1, Poulsbo on EMGD")))
		{
            m_features |= QnGlFunctions::ShadersBroken; /* Shaders are declared but don't work. */
		}

#ifdef Q_OS_MACX
        /* Intel HD 3000 driver handles textures with size > 4096 incorrectly (see bug #3141).
         * To fix that we have to override maximum texture size to 4096 for this graphics adapter. */
        if (m_openGLInfo.vendor.contains(lit("Intel")) &&
            QRegExp(QLatin1String(".*Intel.+3000.*")).exactMatch(m_openGLInfo.renderer))
        {
            qn_glFunctionsGlobal()->overrideMaxTextureSize(4096);
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

	const QnGlFunctions::OpenGLInfo& openGLInfo() const
	{
		return m_openGLInfo;
	}

	static const QnGlFunctions::OpenGLInfo openGLCachedInfo()
	{
		QnMutexLocker lock(&m_mutex);
		return m_openGLInfoCache;
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
	QnGlFunctions::OpenGLInfo m_openGLInfo;

	static QnMutex m_mutex;
	static QnGlFunctions::OpenGLInfo m_openGLInfoCache;
};

// static
QnMutex QnGlFunctionsPrivate::m_mutex;
QnGlFunctions::OpenGLInfo QnGlFunctionsPrivate::m_openGLInfoCache;

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
	return d->openGLInfo();
}

QnGlFunctions::OpenGLInfo QnGlFunctions::openGLCachedInfo()
{
	return QnGlFunctionsPrivate::openGLCachedInfo();
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

    QnGlFunctionsGlobal *global = qn_glFunctionsGlobal();
    if(global) {
        return global->maxTextureSize();
    } else {
        /* We may get called when application is shutting down. */
        return DefaultMaxTextureSize;
    }
}

