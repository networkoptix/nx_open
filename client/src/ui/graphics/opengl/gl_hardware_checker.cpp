#include "gl_hardware_checker.h"

#include <QtCore/QCoreApplication> /* For Q_DECLARE_TR_FUNCTIONS. */
#include <QtOpenGL/QGLWidget>

#include <utils/common/log.h>
#include <utils/common/warnings.h>
#include <utils/common/delete_later.h>

#include <ui/graphics/opengl/gl_functions.h>

namespace {

    class OpenGlDriversErrorMessageDisplay: public QObject {
        Q_DECLARE_TR_FUNCTIONS(OpenGlDriversErrorMessageDisplay);
    public:
        virtual ~OpenGlDriversErrorMessageDisplay() {
            const QString message = tr("We have detected that your video card drivers may be not installed or are out of date.\n"
                "Installing and/or updating your video drivers can substantially increase your system performance when viewing and working with video.\n"
                "For easy instructions on how to install or update your video driver, follow instruction at http://tribaltrouble.com/driversupport.php");
            QMessageBox::critical(NULL, tr("Important Performance Tip"), message, QMessageBox::Ok);
        }
    };

    class OpenGlHardwareErrorMessageDisplay: public QObject {
        Q_DECLARE_TR_FUNCTIONS(OpenGlHardwareErrorMessageDisplay);
    public:
        virtual ~OpenGlHardwareErrorMessageDisplay() {
            const QString message = tr("We have detected that your video card is not supported. You can proceed at your own risk.\n"
                "Installing and/or updating your video drivers may resolve the problem but we cannot guarantee that it will help.\n"
                "For easy instructions on how to install or update your video driver, follow instruction at http://tribaltrouble.com/driversupport.php");

            QMessageBox::critical(NULL, tr("Critical Performance Tip"), message, QMessageBox::Ok);
        }
    };


    class QnGlHardwareCheckerPrivate {
    public:
        QnGlHardwareCheckerPrivate(): m_checked(false) {}
        ~QnGlHardwareCheckerPrivate() {}

        void check(const QGLContext* context);
        
        bool isChecked() const { return m_checked; }
        void setChecked(bool checked = true) { m_checked = checked; }

    private:
        bool m_checked;
    };


    void QnGlHardwareCheckerPrivate::check(const QGLContext* context) {
        if(m_checked)
            return;

        setChecked(true);

        QnGlFunctions functions(context);

        QByteArray extensions = reinterpret_cast<const char *>(glGetString(GL_EXTENSIONS));
        QByteArray version = reinterpret_cast<const char *>(glGetString(GL_VERSION));
        QByteArray renderer = reinterpret_cast<const char *>(glGetString(GL_RENDERER));
        QByteArray vendor = reinterpret_cast<const char *>(glGetString(GL_VENDOR));

        cl_log.log(QString(QLatin1String("OpenGL extensions: %1.")).arg(QLatin1String(extensions.constData())), cl_logINFO);
        cl_log.log(QString(QLatin1String("OpenGL version: %1.")).arg(QLatin1String(version.constData())), cl_logINFO);
        cl_log.log(QString(QLatin1String("OpenGL renderer: %1.")).arg(QLatin1String(renderer.constData())), cl_logINFO);
        cl_log.log(QString(QLatin1String("OpenGL vendor: %1.")).arg(QLatin1String(vendor.constData())), cl_logINFO);

        bool softwareTrouble = false;
        bool hardwareTrouble = false;

        if (functions.features() & QnGlFunctions::OpenGLBroken){
            qnWarning("Intel HD 3000.");
            hardwareTrouble = true;
        }

        if (!(functions.features() & QnGlFunctions::OpenGL1_3)) {
            qnWarning("Multitexturing is not supported.");
            softwareTrouble = true;
        }

        if (version <= QByteArray("1.1.0")) {
            qnWarning("OpenGL version %1 is not supported.", version);
            softwareTrouble = true;
        }

        if (!(functions.features() & QnGlFunctions::ArbPrograms)) {
            qnWarning("OpenGL ARB shaders not supported, using software YUV to RGB conversion.");
            softwareTrouble = true;
        }

        /* Note that message will be shown in destructor, 
         * close to the event loop. */
        if (hardwareTrouble) {
            OpenGlHardwareErrorMessageDisplay *messageDisplay = new OpenGlHardwareErrorMessageDisplay();
            messageDisplay->deleteLater(); 
        } else if(softwareTrouble) {
            OpenGlDriversErrorMessageDisplay *messageDisplay = new OpenGlDriversErrorMessageDisplay();
            messageDisplay->deleteLater();
        }
    }

    Q_GLOBAL_STATIC(QnGlHardwareCheckerPrivate, qn_hardwareCheckerPrivate_instance);

} // anonymous namespace

QnGlHardwareChecker::QnGlHardwareChecker(QGLWidget *parent):
    QObject(parent),
    m_context(parent->context())
{
    parent->installEventFilter(this);
}

QnGlHardwareChecker::~QnGlHardwareChecker() {
    qn_hardwareCheckerPrivate_instance()->check(m_context);
}

bool QnGlHardwareChecker::eventFilter(QObject *, QEvent *event) {
    if (event->type() == QEvent::Paint || event->type() == QEvent::UpdateRequest)
        qnDeleteLater(this);
    return false;
}

