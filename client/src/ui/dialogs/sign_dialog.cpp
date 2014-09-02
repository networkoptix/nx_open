#include "sign_dialog.h"
#include "ui_sign_dialog.h"

#include "plugins/resource/avi/avi_resource.h"
#include "plugins/resource/archive/abstract_archive_stream_reader.h"

#include "camera/gl_renderer.h"
#include "camera/cam_display.h"
#include "camera/sync_dialog_display.h"

#include "ui/workaround/gl_widget_factory.h"
#include "ui/workaround/gl_native_painting.h"
#include "ui/workaround/gl_widget_workaround.h"
#include "ui/graphics/items/resource/decodedpicturetoopengluploadercontextpool.h"
#include "ui/graphics/items/resource/resource_widget_renderer.h"
#include "ui/help/help_topic_accessor.h"
#include "ui/help/help_topics.h"

#include "decoders/video/ffmpeg.h"
#include "export/sign_helper.h"

// TODO: #Elric replace with QnRenderingWidget
class QnSignDialogGlWidget: public QnGLWidget
{
public:
    QnSignDialogGlWidget(const QGLFormat &format, QWidget *parent = NULL, QGLWidget *shareWidget = NULL, Qt::WindowFlags windowFlags = 0):
        QnGLWidget(format, parent, shareWidget, windowFlags)
    {
        m_renderer = 0;
        connect(&m_timer, SIGNAL(timeout()), this, SLOT(update()));
        m_timer.start(16);
        m_textureWidth = 1920;
        m_textureHeight = 1080;
    }

    void setImageSize(int width, int height)
    {
        m_textureWidth = width;
        m_textureHeight = height;
        resizeEvent(0);
    }

    virtual void resizeEvent(QResizeEvent *) override
    {
        m_videoRect = SignDialog::calcVideoRect(width(), height(), m_textureWidth, m_textureHeight);
        if (m_renderer)
            m_renderer->setChannelScreenSize(m_videoRect.size());
        update();
    }

    void setRenderer(QnResourceWidgetRenderer *renderer)
    {
        m_renderer = renderer;
    }

    
    virtual void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        QnGlNativePainting::begin(QGLContext::currentContext(),&painter);
        if (m_renderer)
            m_renderer->paint(0, QRectF(0.0, 0.0, 1.0, 1.0), m_videoRect, 1.0);
        QnGlNativePainting::end(&painter);
    }
    
private:
    QRect m_videoRect;
    QnResourceWidgetRenderer *m_renderer;
    QTimer m_timer;
    double m_textureWidth;
    double m_textureHeight;
};

// ------------------------------------

SignDialog::SignDialog(QnResourcePtr checkResource, QWidget *parent) :
    base_type(parent),
    ui(new Ui::SignDialog),
    m_camDispay(NULL),
    m_reader(NULL),
    m_renderer(NULL),
    m_glWindow(NULL),
    m_srcVideoInfo(0),
    m_layout(0),
    m_requestHandle(-1)
{
    ui->setupUi(this);

    setHelpTopic(this, Qn::Watermark_Help);

    m_layout = new QVBoxLayout(ui->videoSpacer);
    m_layout->setSpacing(0);
    m_layout->setContentsMargins(0,0,0,0);

    m_glWindow = QnGlWidgetFactory::create<QnSignDialogGlWidget>();
    m_layout->addWidget(m_glWindow);
    DecodedPictureToOpenGLUploaderContextPool::instance()->ensureThereAreContextsSharedWith( m_glWindow );
    
    m_srcVideoInfo = new QnSignInfo();

    m_resource = QnAviResourcePtr(new QnAviResource(checkResource->getUrl()));
    m_reader = static_cast<QnAbstractArchiveReader*> (m_resource->createDataProvider(Qn::CR_Default));
    m_reader->setCycleMode(false);
    m_camDispay = new QnSignDialogDisplay(m_resource);

    connect(m_camDispay, SIGNAL(gotSignature(QByteArray, QByteArray)), ui->signInfoLabel, SLOT(at_gotSignature(QByteArray, QByteArray)));
    connect(m_camDispay, SIGNAL(calcSignInProgress(QByteArray, int)), ui->signInfoLabel, SLOT(at_calcSignInProgress(QByteArray, int)));
    connect(m_camDispay, SIGNAL(gotSignatureDescription(QString, QString, QString)), ui->signInfoLabel, SLOT(at_gotSignatureDescription(QString, QString, QString)));
    connect(m_camDispay, SIGNAL(calcSignInProgress(QByteArray, int)), this, SLOT(at_calcSignInProgress(QByteArray, int)));
    connect(m_camDispay, SIGNAL(gotSignature(QByteArray, QByteArray)), this, SLOT(at_gotSignature(QByteArray, QByteArray)));

    connect(m_camDispay, SIGNAL(gotImageSize(int, int)), this, SLOT(at_gotImageSize(int, int)));

    m_renderer = new QnResourceWidgetRenderer(0, m_glWindow->context());
    m_glWindow->setRenderer(m_renderer);
    m_camDispay->addVideoRenderer(1, m_renderer, true);
    m_camDispay->setSpeed(1024*1024);
    m_reader->addDataProcessor(m_camDispay);
    m_reader->start();
    m_camDispay->start();


    connect(ui->buttonBox,                  SIGNAL(accepted()),                     this,   SLOT(accept()));
    connect(ui->buttonBox,                  SIGNAL(rejected()),                     this,   SLOT(reject()));
}

SignDialog::~SignDialog()
{
    m_camDispay->removeVideoRenderer(m_renderer);
    m_renderer->destroyAsync();

    m_reader->pleaseStop();
    m_camDispay->pleaseStop();

    m_reader->stop();
    m_camDispay->stop();
    m_camDispay->clearUnprocessedData();
    
    delete m_camDispay;
    delete m_reader;
    delete m_glWindow;
}

QRect SignDialog::calcVideoRect(double windowWidth, double windowHeight, double textureWidth, double textureHeight)
{
    QRect videoRect;
    double newTextureWidth = static_cast<uint>(textureWidth);

    double windowAspect = windowWidth / windowHeight;
    double textureAspect = textureWidth / textureHeight;
    if (windowAspect > textureAspect)
    {
        // black bars at left and right
        videoRect.setTop(0);
        videoRect.setHeight(windowHeight);
        double scale = windowHeight / textureHeight;
        double scaledWidth = newTextureWidth * scale;
        videoRect.setLeft((windowWidth - scaledWidth) / 2);
        videoRect.setWidth(scaledWidth + 0.5);
    }
    else {
        // black bars at the top and bottom
        videoRect.setLeft(0);
        videoRect.setWidth(windowWidth);
        if (newTextureWidth < windowWidth) {
            double scale = windowWidth / newTextureWidth;
            double scaledHeight = textureHeight * scale;
            videoRect.setTop((windowHeight - scaledHeight) / 2);
            videoRect.setHeight(scaledHeight + 0.5);
        }
        else {
            double newTextureHeight = windowWidth / textureAspect + 0.5;
            videoRect.setTop((windowHeight - newTextureHeight) / 2);
            videoRect.setHeight(newTextureHeight);
        }
    }
    return videoRect;
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
    Q_UNUSED(sign)
    ui->progressBar->setValue(progress);
}

void SignDialog::at_gotImageSize(int width, int height)
{
    if (m_glWindow) 
        m_glWindow->setImageSize(width, height);
    ui->signInfoLabel->setImageSize(width, height);
}

void SignDialog::at_gotSignature(QByteArray calculatedSign, QByteArray signFromFrame)
{
#ifndef SIGN_FRAME_ENABLED
    m_layout->addWidget(m_srcVideoInfo);
    m_glWindow->hide();
    m_srcVideoInfo->setDrawDetailTextMode(true);
    m_srcVideoInfo->at_gotSignature(signFromFrame, calculatedSign); // it is correct. set parameters vise versa here
#endif
}
