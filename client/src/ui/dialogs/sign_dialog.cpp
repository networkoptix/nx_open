#include "sign_dialog.h"
#include "ui_sign_dialog.h"

#include <QEvent>
#include <QtGui/QDataWidgetMapper>
#include <QtGui/QMessageBox>
#include <QtGui/QStandardItemModel>

#include "core/resource/resource.h"
#include "ui/preferences/preferencesdialog.h"
#include "ui/style/skin.h"
#include "ui/workbench/workbench_context.h"
#include "connection_testing_dialog.h"

#include "api/AppServerConnection.h"
#include "api/SessionManager.h"

#include "settings.h"
#include "plugins/resources/archive/avi_files/avi_resource.h"
#include "plugins/resources/archive/abstract_archive_stream_reader.h"
#include "camera/camera.h"
#include "camera/gl_renderer.h"
#include "ui/graphics/items/resource_widget_renderer.h"
#include "camera/camdisplay.h"
#include "decoders/video/ffmpeg.h"
#include "export/sign_helper.h"
#include "camera/sync_dialog_display.h"


class QnSignDialogGlWidget: public QGLWidget
{
public:
    QnSignDialogGlWidget(const QGLFormat & format, QWidget * parent = 0): QGLWidget(format, parent)
    {
        m_renderer = 0;
        connect(&m_timer, SIGNAL(timeout()), this, SLOT(update()));
        m_timer.start(16);
    }

    virtual void  resizeEvent ( QResizeEvent * event ) override
    {
        /*
        // keep aspect ratio 16:9
        int width = event->size().width();
        int height = width * 9.0 / 16.0 + 0.5;
        int hOffset = (event->size().height() - height)/2;

        if (m_renderer)
            m_renderer->setChannelScreenSize(QRect(0, hOffset, width, height));
        */
        static double sar = 1.0;
        double windowHeight = event->size().height();
        double windowWidth = event->size().width();
        double textureWidth = 1920;
        double textureHeight = 1080;
        double newTextureWidth = static_cast<uint>(textureWidth * sar);

        double windowAspect = windowWidth / windowHeight;
        double textureAspect = textureWidth / textureHeight;
        if (windowAspect > textureAspect)
        {
            // black bars at left and right
            m_videoRect.setTop(0);
            m_videoRect.setHeight(windowHeight);
            double scale = windowHeight / textureHeight;
            double scaledWidth = newTextureWidth * scale;
            m_videoRect.setLeft((windowWidth - scaledWidth) / 2);
            m_videoRect.setWidth(scaledWidth + 0.5);
        }
        else {
            // black bars at the top and bottom
            m_videoRect.setLeft(0);
            m_videoRect.setWidth(windowWidth);
            if (newTextureWidth < windowWidth) {
                double scale = windowWidth / newTextureWidth;
                double scaledHeight = textureHeight * scale;
                m_videoRect.setTop((windowHeight - scaledHeight) / 2);
                m_videoRect.setHeight(scaledHeight + 0.5);
            }
            else {
                double newTextureHeight = windowWidth / textureAspect + 0.5;
                m_videoRect.setTop((windowHeight - newTextureHeight) / 2);
                m_videoRect.setHeight(newTextureHeight);
            }
        }

        
        if (m_renderer)
            m_renderer->setChannelScreenSize(m_videoRect.size());
    }

    void setRenderer(QnResourceWidgetRenderer* renderer)
    {
        m_renderer = renderer;
    }

    
    virtual void paintEvent( QPaintEvent * event ) override
    {
        QPainter painter(this);
        painter.beginNativePainting();
        if (m_renderer)
            m_renderer->paint(0, m_videoRect, 1.0);
        painter.endNativePainting();
    }
    
private:
    QRect m_videoRect;
    QnResourceWidgetRenderer* m_renderer;
    QTimer m_timer;
};

// ------------------------------------

SignDialog::SignDialog(const QString& fileName, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SignDialog),
    m_requestHandle(-1),
    m_camDispay(0),
    m_reader(0),
    m_fileName(fileName)
{
    renderer = 0;
    glWindow = 0;


    ui->setupUi(this);

    QVBoxLayout* layout = new QVBoxLayout(ui->videoSpacer);
    layout->setSpacing(0);
    layout->setContentsMargins(0,0,0,10);

    QGLFormat glFormat;
    glFormat.setOption(QGL::SampleBuffers); /* Multisampling. */
    glFormat.setSwapInterval(1); /* Turn vsync on. */
    glWindow = new QnSignDialogGlWidget(glFormat);
    layout->addWidget(glWindow);

    
    //aviRes = QnAviResourcePtr(new QnAviResource("e:/Users/roman76r/blake/FILMS_TEASERS 2_Open for Business Pt 1_kimberely Kane & Dahlia Grey.wmv"));
    aviRes = QnAviResourcePtr(new QnAviResource(m_fileName));
    m_reader = static_cast<QnAbstractArchiveReader*> (aviRes->createDataProvider(QnResource::Role_Default));
    m_reader->setCycleMode(false);
    m_camDispay = new QnSignDialogDisplay();

    connect(m_camDispay, SIGNAL(gotSignature(QByteArray, QByteArray)), ui->signInfoLabel, SLOT(at_gotSignature(QByteArray, QByteArray)));
    connect(m_camDispay, SIGNAL(calcSignInProgress(QByteArray, int)), ui->signInfoLabel, SLOT(at_calcSignInProgress(QByteArray, int)));
    connect(m_camDispay, SIGNAL(calcSignInProgress(QByteArray, int)), this, SLOT(at_calcSignInProgress(QByteArray, int)));

    renderer = new QnResourceWidgetRenderer(1, 0, glWindow->context());
    glWindow->setRenderer(renderer);
    m_camDispay->addVideoChannel(0, renderer, true);
    m_camDispay->setSpeed(1024*1024);
    m_reader->addDataProcessor(m_camDispay);
    m_reader->start();
    m_camDispay->start();


    connect(ui->buttonBox,                  SIGNAL(accepted()),                     this,   SLOT(accept()));
    connect(ui->buttonBox,                  SIGNAL(rejected()),                     this,   SLOT(reject()));
}

SignDialog::~SignDialog()
{
    renderer->beforeDestroy();

    m_reader->pleaseStop();
    m_camDispay->pleaseStop();

    m_reader->stop();
    m_camDispay->stop();
    m_camDispay->clearUnprocessedData();
    
    delete m_camDispay;
    delete renderer;
    delete glWindow;
}

void SignDialog::accept()
{
    done(0);
}

void SignDialog::changeEvent(QEvent *event)
{
    QDialog::changeEvent(event);

    switch (event->type()) {
    case QEvent::LanguageChange:
        ui->retranslateUi(this);
        break;
    default:
        break;
    }
}

void SignDialog::at_calcSignInProgress(QByteArray sign, int progress)
{

}
