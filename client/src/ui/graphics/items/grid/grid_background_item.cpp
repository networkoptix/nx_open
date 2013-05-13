#include "grid_background_item.h"

#include <QPainter>

#include <ui/workbench/workbench_grid_mapper.h>

#include <utils/threaded_image_loader.h>
#include <utils/color_space/yuvconvert.h>

#ifdef _WIN32
//if defined, background is drawn with native API (as gl texture), else - QPainter::drawImage is used
#define NATIVE_PAINT_BACKGROUND
#ifdef NATIVE_PAINT_BACKGROUND
//!use YUV 420 with alpha plane
#define USE_YUVA420
#endif
#endif


QnGridBackgroundItem::QnGridBackgroundItem(QGraphicsItem *parent):
    QGraphicsObject(parent),
    m_imageSize(1, 1),
    m_imageOpacity(0.7),
    m_cache(new QnAppServerImageCache(this)),
    m_imgUploaded(false),
    m_imageStatus(None)
{
    setAcceptedMouseButtons(0);

    connect(m_cache, SIGNAL(fileDownloaded(QString, bool)), this, SLOT(at_imageLoaded(QString, bool)));
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
    return m_rect;
}

void QnGridBackgroundItem::updateDisplay() {
    if (m_imageFilename.isEmpty()) {
        setOpacity(0.0);
        return;
    } else {
        setOpacity(m_imageOpacity);
        updateGeometry();
    }

    if (m_imageStatus != None)
        return;
    m_imageStatus = Loading;
    m_cache->downloadFile(m_imageFilename);
}

const QRectF& QnGridBackgroundItem::viewportRect() const {
    return m_rect;
}

void QnGridBackgroundItem::setViewportRect(const QRectF &rect) {
    prepareGeometryChange();
    m_rect = rect;
}

QnWorkbenchGridMapper* QnGridBackgroundItem::mapper() const {
    return m_mapper.data();
}

void QnGridBackgroundItem::setMapper(QnWorkbenchGridMapper *mapper) {
    m_mapper = mapper;
    connect(mapper,     SIGNAL(originChanged()),    this,   SLOT(updateGeometry()));
    connect(mapper,     SIGNAL(cellSizeChanged()),  this,   SLOT(updateGeometry()));
    connect(mapper,     SIGNAL(spacingChanged()),   this,   SLOT(updateGeometry()));

    updateGeometry();
}

QString QnGridBackgroundItem::imageFilename() const {
    return m_imageFilename;
}

void QnGridBackgroundItem::setImageFilename(const QString &imageFilename) {
    if (m_imageFilename == imageFilename)
        return;
    m_imageFilename = imageFilename;
    m_imageStatus = None;

#ifdef NATIVE_PAINT_BACKGROUND
    m_imgAsFrame = QSharedPointer<CLVideoDecoderOutput>();
#else
    m_image = QImage();
#endif
}

QSize QnGridBackgroundItem::imageSize() const {
    return m_imageSize;
}

void QnGridBackgroundItem::setImageSize(const QSize &imageSize) {
    if (m_imageSize == imageSize)
        return;
    m_imageSize = imageSize;
}

qreal QnGridBackgroundItem::imageOpacity() const {
    return m_imageOpacity;
}

void QnGridBackgroundItem::setImageOpacity(qreal value) {
    m_imageOpacity = qBound(0.0, value, 1.0);
}

QRect QnGridBackgroundItem::sceneBoundingRect() const {
    if (m_imageFilename.isEmpty())
        return QRect();
    return m_sceneBoundingRect;
}

void QnGridBackgroundItem::updateGeometry() {
    if(mapper() == NULL)
        return;

    int left = m_imageSize.width() / 2;
    int top =  m_imageSize.height() / 2;
    m_sceneBoundingRect = QRect(-left, -top, m_imageSize.width(), m_imageSize.height());

    QRectF targetRect = mapper()->mapFromGrid(m_sceneBoundingRect);
    setViewportRect(targetRect);
}

void QnGridBackgroundItem::at_imageLoaded(const QString& filename, bool ok) {
    if (filename != m_imageFilename || m_imageStatus != Loading)
        return;
    m_imageStatus = Loaded;
    if (!ok)
        return;

    if (m_imagesMemCache.contains(m_imageFilename)) {
        setImage(m_imagesMemCache[m_imageFilename]);
        return;
    }

    QnThreadedImageLoader* loader = new QnThreadedImageLoader(this);
    loader->setInput(m_cache->getFullPath(filename));
    loader->setSize(m_cache->getMaxImageSize());
    connect(loader, SIGNAL(finished(QImage)), this, SLOT(setImage(QImage)));
    loader->start();
}

void QnGridBackgroundItem::setImage(const QImage &image) {
    if (m_imageStatus != Loaded)    // image name was changed during load
        return;

    if (!m_imagesMemCache.contains(m_imageFilename)) {
        m_imagesMemCache.insert(m_imageFilename, image);
    }

#ifdef NATIVE_PAINT_BACKGROUND
    //converting image to YUV format
    m_imgAsFrame = QSharedPointer<CLVideoDecoderOutput>( new CLVideoDecoderOutput() );

    //adding stride to source data
    //TODO: #ak it is possible to remove this copying and optimize loading by using ffmpeg to load picture files
    const unsigned int requiredImgXStride = qPower2Ceil( (unsigned int)image.width()*4, X_STRIDE_FOR_SSE_CONVERT_UTILS );
    quint8* alignedImgBuffer = (quint8*)qMallocAligned( requiredImgXStride*image.height(), X_STRIDE_FOR_SSE_CONVERT_UTILS );
    //copying image data to aligned buffer
    for( int y = 0; y < image.height(); ++y )
        memcpy( alignedImgBuffer + requiredImgXStride*y, image.constScanLine(y), image.width()*image.depth()/8 );

#ifdef USE_YUVA420
    m_imgAsFrame->reallocate( image.width(), image.height(), PIX_FMT_YUVA420P );
    bgra_to_yva12_sse2_intr(
        alignedImgBuffer,
        requiredImgXStride,
        m_imgAsFrame->data[0],
        m_imgAsFrame->data[1],
        m_imgAsFrame->data[2],
        m_imgAsFrame->data[3],
        m_imgAsFrame->linesize[0],
        m_imgAsFrame->linesize[1],
        m_imgAsFrame->linesize[3],
        image.width(),
        image.height(),
        false );
#else
    m_imgAsFrame->reallocate( image.width(), image.height(), PIX_FMT_YUV420P );
    bgra_to_yv12_sse2_intr(
        alignedImgBuffer,
        requiredImgXStride,
        m_imgAsFrame->data[0],
        m_imgAsFrame->data[1],
        m_imgAsFrame->data[2],
        m_imgAsFrame->linesize[0],
        m_imgAsFrame->linesize[1],
        image.width(),
        image.height(),
        false );
#endif

    qFreeAligned( alignedImgBuffer );

    //image has to be uploaded before next paint
    m_imgUploaded = false;

    m_renderer.reset();
    m_imgUploader.reset();
#else
    m_image = image;
#endif
}

void QnGridBackgroundItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
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

    if( !m_imgUploaded )
    {
        //uploading image to opengl texture
        m_imgUploader->uploadDecodedPicture( m_imgAsFrame );
        m_imgUploaded = true;
    }

    painter->beginNativePainting();

    if( painter->opacity() < 1.0 )
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    m_imgUploader->setOpacity( painter->opacity() );
    m_renderer->paint(
        QRectF(0, 0, 1, 1),
        m_rect );
    painter->endNativePainting();
#else
    if (!m_image.isNull())
        painter->drawImage(m_rect, m_image);
#endif
}
