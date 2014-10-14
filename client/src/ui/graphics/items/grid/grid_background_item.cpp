#include "grid_background_item.h"

#include <QtGui/QPainter>

#include <utils/math/math.h>
#include <utils/color_space/yuvconvert.h>
#include <utils/threaded_image_loader.h>
#include <utils/local_file_cache.h>
#include <utils/app_server_image_cache.h>

#include <client/client_settings.h>

#include <core/resource/user_resource.h>
#include <core/resource/layout_resource.h>

#include <ui/workaround/gl_native_painting.h>
#include <ui/workbench/workbench_grid_mapper.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_layout_snapshot_manager.h>
#include <ui/workbench/workbench_display.h>

#include <utils/common/warnings.h>

#ifdef _WIN32
//if defined, background is drawn with native API (as gl texture), else - QPainter::drawImage is used
#define NATIVE_PAINT_BACKGROUND
#ifdef NATIVE_PAINT_BACKGROUND
//!use YUV 420 with alpha plane
#define USE_YUVA420
#endif
#endif


namespace {
    enum ImageStatus {
        None,
        Loading,
        Loaded
    };

    const char *loaderFilenamePropertyName = "_qn_loaderFilename";
}

class QnGridBackgroundItemPrivate {
public:
    QnGridBackgroundItemPrivate():
        imageSize(1, 1),
        imageAspectRatio(1),
        imageOpacity(0.7),
        imageMode(Qn::StretchImage),
        isDefaultBackground(true),
        imageIsLocal(true),
        connected(false),
#ifdef NATIVE_PAINT_BACKGROUND
        imgUploaded(false),
#endif
        imageStatus(ImageStatus::None)
    {}
    virtual ~QnGridBackgroundItemPrivate() {}

    bool imageIsVisible() const {
        return
                !qFuzzyIsNull(imageOpacity) &&
                !imageFilename.isEmpty() &&
                (imageIsLocal || connected);

    }

    QString imageFilename;
    QSize imageSize;
    qreal imageAspectRatio;
    qreal imageOpacity;
    Qn::ImageBehaviour imageMode;

    /** Layout has no own background, using default image (if exists). */
    bool isDefaultBackground;

    bool imageIsLocal;
    bool connected;

    QRectF rect;
    QRect sceneBoundingRect;

#ifdef NATIVE_PAINT_BACKGROUND
    bool imgUploaded;
#else
    QImage image;
#endif
    ImageStatus imageStatus;
    QHash<QString, QImage> imagesMemCache;  //TODO: #GDM #Common #High replace with QCache
};



QnGridBackgroundItem::QnGridBackgroundItem(QGraphicsItem *parent, QnWorkbenchContext *context):
    QGraphicsObject(parent),
    QnWorkbenchContextAware(NULL, context),
    d_ptr(new QnGridBackgroundItemPrivate())
{
    setAcceptedMouseButtons(0);

    connect(this->context()->instance<QnLocalFileCache>(), SIGNAL(fileDownloaded(QString, bool)), this, SLOT(at_imageLoaded(QString, bool)));
    connect(this->context()->instance<QnAppServerImageCache>(), SIGNAL(fileDownloaded(QString, bool)), this, SLOT(at_imageLoaded(QString, bool)));
    connect(this->context(), SIGNAL(userChanged(QnUserResourcePtr)), this, SLOT(at_context_userChanged()));

    connect(qnSettings->notifier(QnClientSettings::BACKGROUND_MODE),            &QnPropertyNotifier::valueChanged, this, &QnGridBackgroundItem::updateDefaultBackground);
    connect(qnSettings->notifier(QnClientSettings::BACKGROUND_IMAGE),           &QnPropertyNotifier::valueChanged, this, &QnGridBackgroundItem::updateDefaultBackground);
    connect(qnSettings->notifier(QnClientSettings::BACKGROUND_IMAGE_OPACITY),   &QnPropertyNotifier::valueChanged, this, &QnGridBackgroundItem::updateDefaultBackground);
    connect(qnSettings->notifier(QnClientSettings::BACKGROUND_IMAGE_MODE),      &QnPropertyNotifier::valueChanged, this, &QnGridBackgroundItem::updateDefaultBackground);

    /* Don't disable this item here. When disabled, it starts accepting wheel events
     * (and probably other events too). Looks like a Qt bug. */
}

QnGridBackgroundItem::~QnGridBackgroundItem() {
#ifdef NATIVE_PAINT_BACKGROUND
    m_renderer.reset();
    m_imgUploader.reset();
#endif
}

QRectF QnGridBackgroundItem::boundingRect() const {
    Q_D(const QnGridBackgroundItem);
    return d->rect;
}

void QnGridBackgroundItem::updateDefaultBackground(int settingsId) {
    Q_D(QnGridBackgroundItem);
    if (!d->isDefaultBackground)
        return;

    switch (settingsId) {
    case QnClientSettings::BACKGROUND_MODE:
    case QnClientSettings::BACKGROUND_IMAGE:
        {
            QString filename = qnSettings->backgroundMode() == Qn::ImageBackground
                ? qnSettings->backgroundImage()
                : QString();

            if (d->imageFilename == filename) 
                return;

            d->imageFilename = filename;
            d->imageStatus = ImageStatus::None;
            m_imgAsFrame = QSharedPointer<CLVideoDecoderOutput>();
            break;
        }
    case QnClientSettings::BACKGROUND_IMAGE_OPACITY:
        {
            qreal opacity = qnSettings->backgroundImageOpacity();
            if (qFuzzyCompare(d->imageOpacity, opacity))
                return;
            d->imageOpacity = opacity;
            break;
        }
    case QnClientSettings::BACKGROUND_IMAGE_MODE:
        {
            Qn::ImageBehaviour mode = qnSettings->backgroundImageMode();
            if (d->imageMode == mode)
                return;
            d->imageMode = mode;
            break;
        }
    default:
        return;
    }

    updateDisplay();
}

void QnGridBackgroundItem::updateDisplay() {
    Q_D(QnGridBackgroundItem);

    if (!d->imageIsVisible()) {
        setOpacity(0.0);
        return;
    } else {
        setOpacity(d->imageOpacity);
        updateGeometry();
    }

    if (d->imageStatus != ImageStatus::None)
        return;
    d->imageStatus = ImageStatus::Loading;
    cache()->downloadFile(d->imageFilename);
}

const QRectF& QnGridBackgroundItem::viewportRect() const {
    Q_D(const QnGridBackgroundItem);
    return d->rect;
}

void QnGridBackgroundItem::setViewportRect(const QRectF &rect) {
    Q_D(QnGridBackgroundItem);
    prepareGeometryChange();
    d->rect = rect;
}

QnWorkbenchGridMapper* QnGridBackgroundItem::mapper() const {
    return m_mapper.data();
}

void QnGridBackgroundItem::setMapper(QnWorkbenchGridMapper *mapper) {
    if (m_mapper.data())
        disconnect(m_mapper.data(), 0, this, 0);
    m_mapper = mapper;
    if (mapper) {
        connect(mapper,     SIGNAL(originChanged()),    this,   SLOT(updateGeometry()));
        connect(mapper,     SIGNAL(cellSizeChanged()),  this,   SLOT(updateGeometry()));
        connect(mapper,     SIGNAL(spacingChanged()),   this,   SLOT(updateGeometry()));
    }

    updateGeometry();
}

void QnGridBackgroundItem::update(const QnLayoutResourcePtr &layout) {
    Q_D(QnGridBackgroundItem);

    bool isDefaultBackground = layout->backgroundImageFilename().isEmpty();

    bool isExportedLayout = snapshotManager()->isFile(layout);

    QString filename = isDefaultBackground
        ? qnSettings->backgroundMode() == Qn::ImageBackground
            ? qnSettings->backgroundImage()
            : QString()
        : layout->backgroundImageFilename();

    QSize imageSize = isDefaultBackground
        ? QSize(1, 1)
        : layout->backgroundSize();
       
    qreal opacity = isDefaultBackground
        ? qnSettings->backgroundImageOpacity()
        : qBound(0.0, layout->backgroundOpacity(), 1.0);

    bool hasChanges =
            (d->isDefaultBackground != isDefaultBackground) ||
            (d->imageIsLocal != (isExportedLayout || isDefaultBackground) ) ||
            (d->imageFilename != filename) ||
            (d->imageSize != imageSize) ||
            (!qFuzzyCompare(d->imageOpacity, opacity));

    if (hasChanges) {
        d->isDefaultBackground = isDefaultBackground;
        d->imageIsLocal = isExportedLayout || isDefaultBackground;
        d->imageFilename = filename;
        d->imageSize = imageSize;
        d->imageOpacity = opacity;
        d->imageStatus = ImageStatus::None;
#ifdef NATIVE_PAINT_BACKGROUND
        m_imgAsFrame = QSharedPointer<CLVideoDecoderOutput>();
#else
        d->image = QImage();
#endif
    }

    updateDisplay();
}

QRect QnGridBackgroundItem::sceneBoundingRect() const {
    Q_D(const QnGridBackgroundItem);
    if (!d->imageIsVisible())
        return QRect();
    return d->sceneBoundingRect;
}

void QnGridBackgroundItem::updateGeometry() {
    Q_D(QnGridBackgroundItem);

    if (d->isDefaultBackground) {
        d->sceneBoundingRect = QRect();
        const qreal d = std::numeric_limits<qreal>::max() / 4;
        setViewportRect(QRectF(QPointF(-d, -d), QPointF(d, d)));
        return;
    }

    if(mapper() == NULL)
        return;

    int left = d->imageSize.width() / 2;
    int top =  d->imageSize.height() / 2;
    d->sceneBoundingRect = QRect(-left, -top, d->imageSize.width(), d->imageSize.height());

    QRectF targetRect = mapper()->mapFromGrid(d->sceneBoundingRect);
    setViewportRect(targetRect);
}

void QnGridBackgroundItem::at_context_userChanged() {
    Q_D(QnGridBackgroundItem);
    d->connected = context()->user() != NULL;
    updateDisplay();
}

void QnGridBackgroundItem::at_imageLoaded(const QString& filename, bool ok) {
    Q_D(QnGridBackgroundItem);
    if (filename != d->imageFilename || d->imageStatus != ImageStatus::Loading)
        return;
    d->imageStatus = ImageStatus::Loaded;
    if (!ok)
        return;

    if (d->imagesMemCache.contains(d->imageFilename)) {
        setImage(d->imagesMemCache[d->imageFilename]);
        return;
    }

    QnThreadedImageLoader* loader = new QnThreadedImageLoader(this);
    loader->setInput(cache()->getFullPath(filename));
    loader->setSize(cache()->getMaxImageSize());
    loader->setProperty(loaderFilenamePropertyName, d->imageFilename);
    connect(loader, SIGNAL(finished(QImage)), this, SLOT(setImage(QImage)));
    loader->start();
}

QnAppServerImageCache* QnGridBackgroundItem::cache() {
    Q_D(const QnGridBackgroundItem);

    if (d->imageIsLocal)
        return context()->instance<QnLocalFileCache>();
    return context()->instance<QnAppServerImageCache>();
}

void QnGridBackgroundItem::setImage(const QImage &image) {
    Q_D(QnGridBackgroundItem);

    QString filename = sender()
            ? sender()->property(loaderFilenamePropertyName).toString()
            : QString();

    if (!filename.isEmpty() && filename != d->imageFilename)
        return; // race condition

    if (d->imageStatus != ImageStatus::Loaded)    // image name was changed during load
        return;

    if (!d->imagesMemCache.contains(d->imageFilename)) {
        d->imagesMemCache.insert(d->imageFilename, image);
    }

    d->imageAspectRatio = QnGeometry::aspectRatio(image.size());

#ifdef NATIVE_PAINT_BACKGROUND
    //converting image to ARGB32 since we cannot convert to YUV from monochrome, indexed, etc..
    const QImage* imgToLoad = NULL;
    std::auto_ptr<QImage> tempImgAp;
    if( image.format() != QImage::Format_ARGB32 )
    {
        tempImgAp.reset( new QImage() );
        *tempImgAp = image.convertToFormat( QImage::Format_ARGB32 );
        imgToLoad = tempImgAp.get();
    }
    else
    {
        imgToLoad = &image;
    }

    //converting image to YUV format
    m_imgAsFrame = QSharedPointer<CLVideoDecoderOutput>( new CLVideoDecoderOutput() );

    //adding stride to source data
    //TODO: #ak it is possible to remove this copying and optimize loading by using ffmpeg to load picture files
    const unsigned int requiredImgXStride = qPower2Ceil( (unsigned int)imgToLoad->width()*4, X_STRIDE_FOR_SSE_CONVERT_UTILS );
    quint8* alignedImgBuffer = (quint8*)qMallocAligned( requiredImgXStride*imgToLoad->height(), X_STRIDE_FOR_SSE_CONVERT_UTILS );
    //copying image data to aligned buffer
    for( int y = 0; y < imgToLoad->height(); ++y )
        memcpy( alignedImgBuffer + requiredImgXStride*y, imgToLoad->constScanLine(y), imgToLoad->width()*imgToLoad->depth()/8 );

#ifdef USE_YUVA420
    m_imgAsFrame->reallocate( imgToLoad->width(), imgToLoad->height(), PIX_FMT_YUVA420P );
    bgra_to_yva12_simd_intr(
        alignedImgBuffer,
        requiredImgXStride,
        m_imgAsFrame->data[0],
        m_imgAsFrame->data[1],
        m_imgAsFrame->data[2],
        m_imgAsFrame->data[3],
        m_imgAsFrame->linesize[0],
        m_imgAsFrame->linesize[1],
        m_imgAsFrame->linesize[3],
        imgToLoad->width(),
        imgToLoad->height(),
        false );
#else
    m_imgAsFrame->reallocate( imgToLoad->width(), imgToLoad->height(), PIX_FMT_YUV420P );
    bgra_to_yv12_sse2_intr(
        alignedImgBuffer,
        requiredImgXStride,
        m_imgAsFrame->data[0],
        m_imgAsFrame->data[1],
        m_imgAsFrame->data[2],
        m_imgAsFrame->linesize[0],
        m_imgAsFrame->linesize[1],
        imgToLoad->width(),
        imgToLoad->height(),
        false );
#endif

    qFreeAligned( alignedImgBuffer );

    //image has to be uploaded before next paint
    d->imgUploaded = false;

    m_renderer.reset();
    m_imgUploader.reset();
#else
    d->image = image;
#endif
}

void QnGridBackgroundItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    Q_D(QnGridBackgroundItem);

    QRectF targetRect = d->rect;
    if (d->isDefaultBackground) {
        switch (d->imageMode) {
        case Qn::FitImage:
            targetRect = QnGeometry::expanded(d->imageAspectRatio, display()->viewportGeometry(), Qt::KeepAspectRatio, Qt::AlignCenter);
            break;
        case Qn::CropImage:
            targetRect = QnGeometry::expanded(d->imageAspectRatio, display()->viewportGeometry(), Qt::KeepAspectRatioByExpanding, Qt::AlignCenter);
            break;
        default:
            targetRect = display()->viewportGeometry();
            break;
        }
    }

#ifdef NATIVE_PAINT_BACKGROUND
    if( !m_imgAsFrame )
        return;

    if( !m_imgUploader.get() )
    {
        m_imgUploader.reset( new DecodedPictureToOpenGLUploader(QGLContext::currentContext()) );
        m_renderer.reset( new QnGLRenderer(QGLContext::currentContext(), *m_imgUploader) );
        m_imgUploader->setYV12ToRgbShaderUsed(m_renderer->isYV12ToRgbShaderUsed());
        m_imgUploader->setNV12ToRgbShaderUsed(m_renderer->isNV12ToRgbShaderUsed());
    }

    if( !d->imgUploaded )
    {
        //uploading image to opengl texture
        m_imgUploader->uploadDecodedPicture( m_imgAsFrame );
        d->imgUploaded = true;
    }

    QnGlNativePainting::begin(QGLContext::currentContext(),painter);

    if( m_imgAsFrame->format == PIX_FMT_YUVA420P || m_imgAsFrame->format == PIX_FMT_RGBA )
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    m_imgUploader->setOpacity( painter->opacity() );

    m_renderer->paint(QRectF(0, 0, 1, 1), targetRect);

    if( m_imgAsFrame->format == PIX_FMT_YUVA420P || m_imgAsFrame->format == PIX_FMT_RGBA )
        glDisable(GL_BLEND);

    QnGlNativePainting::end(painter);

#else
    if (!d->image.isNull())
        painter->drawImage(targetRect, d->image);
#endif
}
