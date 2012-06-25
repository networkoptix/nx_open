#include "gl_hardware_check.h"
#include "ui\graphics\opengl\gl_functions.h"
#include "utils\common\log.h"
#include "utils\common\warnings.h"

namespace {
    class OpenGlDriversErrorMessageDisplay: public QObject {
    public:
        virtual ~OpenGlDriversErrorMessageDisplay() {
            const QString message = tr("We have detected that your video card drivers may be not installed or out of date.\n"
                "Installing and/or updating your video drivers can substantially increase your system performance when viewing and working with video.\n"
                "For easy instructions on how to install or update your video driver, follow instruction at http://tribaltrouble.com/driversupport.php");
            QMessageBox::critical(NULL, tr("Important Performance Tip"), message, QMessageBox::Ok);
        }
    };

    class OpenGlHardwareErrorMessageDisplay: public QObject {
    public:
        virtual ~OpenGlHardwareErrorMessageDisplay() {
            const QString message = tr("We have detected that your video card is no supported. You can use our software at your own risk.\n"
                "Installing and/or updating your video drivers can resolve this problem but we have no guarantee that it will help.\n"
                "For easy instructions on how to install or update your video driver, follow instruction at http://tribaltrouble.com/driversupport.php");

            QMessageBox::critical(NULL, tr("Critical Performance Tip"), message, QMessageBox::Ok);
        }
    };


    class HardwareCheckPrivate
    {
    private:
        bool m_checked;
    public:
        HardwareCheckPrivate():m_checked(false){}
        ~HardwareCheckPrivate(void){}
        void check(const QGLContext* context);
        bool isChecked(){return m_checked;}
        void setChecked(){m_checked = true;}
    };


    Q_GLOBAL_STATIC(HardwareCheckPrivate, instance);

    void HardwareCheckPrivate::check(const QGLContext* context){
        if (!instance()->isChecked()){
            instance()->setChecked();

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

            if (hardwareTrouble){
                OpenGlHardwareErrorMessageDisplay *messageDisplay = new OpenGlHardwareErrorMessageDisplay();
                messageDisplay->deleteLater(); /* Message will be shown in destructor, close to the event loop. */
            }
            else // only one message dialog should be displayed
            if(softwareTrouble) {
                OpenGlDriversErrorMessageDisplay *messageDisplay = new OpenGlDriversErrorMessageDisplay();
                messageDisplay->deleteLater(); /* Message will be shown in destructor, close to the event loop. */
            }
        }
    }
} // anonymous namespace

HardwareCheckEventFilter::~HardwareCheckEventFilter(){
    instance()->check(m_context);
}

bool HardwareCheckEventFilter::eventFilter(QObject *, QEvent *e){
    if (e->type() == QEvent::Paint || e->type() == QEvent::UpdateRequest)
        deleteLater();
    return false;
}

