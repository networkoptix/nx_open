#include "grid_background_item.h"

#include <QtGui/QPainter>

#include <common/common_module.h>

#include <utils/math/math.h>
#include <utils/color_space/yuvconvert.h>
#include <utils/threaded_image_loader.h>
#include <nx/client/desktop/utils/local_file_cache.h>
#include <nx/client/desktop/utils/server_image_cache.h>

#include <client/client_settings.h>

#include <core/resource/user_resource.h>
#include <core/resource/layout_resource.h>

#include <ui/workaround/gl_native_painting.h>
#include <ui/workbench/workbench_grid_mapper.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_layout.h>

#include <utils/common/warnings.h>

#if defined(_WIN32)
    /**
     * if defined, background is drawn with native API (as gl texture),
     * else - QPainter::drawImage is used
     */
    #define NATIVE_PAINT_BACKGROUND
    #if defined(NATIVE_PAINT_BACKGROUND)
        // Use YUV 420 with alpha plane
        #define USE_YUVA420
    #endif
#endif

using namespace nx::client::desktop;

namespace {

static const auto loaderFilenamePropertyName = "_qn_loaderFilename";

enum class ImageStatus
{
    None,
    Loading,
    Loaded
};

enum class BackgroundType
{
    Default,
    Image,
    Special
};

BackgroundType backgroundTypeFromLayout(const QnLayoutResourcePtr& layout)
{
    const auto workbenchLayout = QnWorkbenchLayout::instance(layout);
    if (workbenchLayout && workbenchLayout->flags().testFlag(QnLayoutFlag::SpecialBackground))
        return BackgroundType::Special;

    if (layout->backgroundImageFilename().isEmpty())
        return BackgroundType::Default;

    return BackgroundType::Image;
}

//////////////////////////////////////////////////////////////////////////////////////////////////

static const QSize kDefaultImageSize = QSize(1, 1);
struct BackgroundImageData
{
    QString fileName;
    QSize size = kDefaultImageSize;
    qreal opacity = 1;
    Qn::ImageBehaviour mode = Qn::ImageBehaviour::Stretch;
    bool isLocal = true;

    BackgroundImageData() = default;

    BackgroundImageData(
        const QString& fileName,
        const QSize& size,
        qreal opacity,
        Qn::ImageBehaviour mode,
        bool isLocal);

    static BackgroundImageData getForDefaultType();
    static BackgroundImageData getForImageType(const QnLayoutResourcePtr& layout);

    bool imageIsVisible(bool connected) const;

    bool operator==(const BackgroundImageData& other) const;
    bool operator!=(const BackgroundImageData& other) const;
};

BackgroundImageData::BackgroundImageData(
    const QString& fileName,
    const QSize& size,
    qreal opacity,
    Qn::ImageBehaviour mode,
    bool isLocal)
    :
    fileName(fileName),
    size(size),
    opacity(opacity),
    mode(mode),
    isLocal(isLocal)
{
}

bool BackgroundImageData::imageIsVisible(bool connected) const
{
    return (isLocal || connected)
        && !fileName.isEmpty()
        && !qFuzzyIsNull(opacity);
}

BackgroundImageData BackgroundImageData::getForDefaultType()
{
    const auto background = qnSettings->backgroundImage();
    return BackgroundImageData(
        background.name,
        kDefaultImageSize,
        background.actualImageOpacity(),
        background.mode,
        true); //< Image is always local for default background image.
}

BackgroundImageData BackgroundImageData::getForImageType(const QnLayoutResourcePtr& layout)
{
    const auto background = qnSettings->backgroundImage();
    return BackgroundImageData(
        layout->backgroundImageFilename(),
        layout->backgroundSize(),
        qBound<qreal>(0, layout->backgroundOpacity(), 1),
        background.mode,
        layout->isFile()); //< Image is local if layout is exported.
}

bool BackgroundImageData::operator==(const BackgroundImageData& other) const
{
    return (fileName == other.fileName)
        && (size == other.size)
        && (mode == other.mode)
        && (isLocal == other.isLocal)
        && qFuzzyEquals(opacity, other.opacity);
}

bool BackgroundImageData::operator!=(const BackgroundImageData& other) const
{
    return !(*this == other);
}

} // namespace

//////////////////////////////////////////////////////////////////////////////////////////////////
class QnGridBackgroundItemPrivate
{
public:
    BackgroundType backgroundType = BackgroundType::Default;

    qreal imageAspectRatio = 1.0;
    BackgroundImageData imageData;
    ImageStatus imageStatus = ImageStatus::None;


    QRectF rect;
    QRect sceneBoundingRect;
    QHash<QString, QImage> imagesMemCache;  // TODO: #GDM #Common #High replace with QCache

    #if defined(NATIVE_PAINT_BACKGROUND)
        bool imgUploaded = false;
    #else
        QImage image;
    #endif
    bool connected;
};

//////////////////////////////////////////////////////////////////////////////////////////////////

QnGridBackgroundItem::QnGridBackgroundItem(QGraphicsItem* parent, QnWorkbenchContext* context):
    base_type(parent),
    QnWorkbenchContextAware(nullptr, context),
    d_ptr(new QnGridBackgroundItemPrivate()),
    m_panelColor(Qt::black)
{
    setAcceptedMouseButtons(Qt::NoButton);

    const auto imageLoaded =
        [this](const QString& filename, ServerFileCache::OperationResult status)
        {
            at_imageLoaded(filename, status == ServerFileCache::OperationResult::ok);
        };

    const auto localFilesCache = context->instance<LocalFileCache>();
    connect(localFilesCache, &ServerFileCache::fileDownloaded, this, imageLoaded);

    const auto appServerImageCache = context->instance<ServerImageCache>();
    connect(appServerImageCache, &ServerFileCache::fileDownloaded, this, imageLoaded);

    connect(commonModule(), &QnCommonModule::remoteIdChanged,
        this, &QnGridBackgroundItem::updateConnectedState);

    const auto notifier = qnSettings->notifier(QnClientSettings::BACKGROUND_IMAGE);
    connect(notifier, &QnPropertyNotifier::valueChanged,
        this, &QnGridBackgroundItem::updateDefaultBackground);

    /**
     * Don't disable this item here. When disabled, it starts accepting wheel events
     * (and probably other events too). Looks like a Qt bug.
     */

    updateConnectedState();
    updateDefaultBackground();
}

QnGridBackgroundItem::~QnGridBackgroundItem()
{
    #ifdef NATIVE_PAINT_BACKGROUND
        m_renderer.reset();
        m_imgUploader.reset();
    #endif
}

QRectF QnGridBackgroundItem::boundingRect() const
{
    Q_D(const QnGridBackgroundItem);
    return d->rect;
}

void QnGridBackgroundItem::updateDefaultBackground()
{
    Q_D(QnGridBackgroundItem);
    if (d->backgroundType != BackgroundType::Default)
        return;

    const QnBackgroundImage background = qnSettings->backgroundImage();

    bool hasChanges = false;
    if (d->imageData.fileName != background.name)
    {
        d->imageData.fileName = background.name;
        d->imageStatus = ImageStatus::None;
        #ifdef NATIVE_PAINT_BACKGROUND
            m_imgAsFrame = QSharedPointer<CLVideoDecoderOutput>();
        #else
            d->image = QImage();
        #endif
        hasChanges = true;
    }

    if (!qFuzzyEquals(d->imageData.opacity, background.actualImageOpacity()))
    {
        d->imageData.opacity = background.actualImageOpacity();
        hasChanges = true;
    }

    if (d->imageData.mode != background.mode)
    {
        d->imageData.mode = background.mode;
        hasChanges = true;
    }

    /* Early return if nothing image-related changed. */
    if (!hasChanges)
        return;

    updateDisplay();
}

void QnGridBackgroundItem::updateDisplay()
{
    Q_D(QnGridBackgroundItem);

    if (d->backgroundType == BackgroundType::Special)
    {
        setOpacity(d->imageData.opacity);
        updateGeometry();
        return;
    }

    if (!d->imageData.imageIsVisible(d->connected))
    {
        setOpacity(0.0);
        return;
    }
    else
    {
        setOpacity(d->imageData.opacity);
        updateGeometry();
    }

    if (d->imageStatus != ImageStatus::None)
        return;

    d->imageStatus = ImageStatus::Loading;
    cache()->downloadFile(d->imageData.fileName);
}

const QRectF& QnGridBackgroundItem::viewportRect() const
{
    Q_D(const QnGridBackgroundItem);
    return d->rect;
}

void QnGridBackgroundItem::setViewportRect(const QRectF& rect)
{
    Q_D(QnGridBackgroundItem);
    prepareGeometryChange();
    d->rect = rect;
}

QnWorkbenchGridMapper* QnGridBackgroundItem::mapper() const
{
    return m_mapper.data();
}

void QnGridBackgroundItem::setMapper(QnWorkbenchGridMapper* mapper)
{
    if (m_mapper.data())
        disconnect(m_mapper.data(), nullptr, this, nullptr);

    m_mapper = mapper;
    if (mapper)
    {
        connect(mapper, &QnWorkbenchGridMapper::originChanged,
            this, &QnGridBackgroundItem::updateGeometry);
        connect(mapper, &QnWorkbenchGridMapper::cellSizeChanged,
            this, &QnGridBackgroundItem::updateGeometry);
        connect(mapper, &QnWorkbenchGridMapper::spacingChanged,
            this, &QnGridBackgroundItem::updateGeometry);
    }

    updateGeometry();
}

void QnGridBackgroundItem::update(const QnLayoutResourcePtr& layout)
{
    Q_D(QnGridBackgroundItem);

    const auto type = backgroundTypeFromLayout(layout);
    if (type == d->backgroundType && type == BackgroundType::Special)
        return; //< Nothing couldn't be changed here

    const auto data =
        [layout, type]() -> BackgroundImageData
        {
            switch(type)
            {
                case BackgroundType::Special:
                    return BackgroundImageData();
                case BackgroundType::Image:
                    return BackgroundImageData::getForImageType(layout);
                case BackgroundType::Default:
                    return BackgroundImageData::getForDefaultType();
                default:
                    NX_ASSERT(false, "Wrong BackgroundType value passed");
                    return BackgroundImageData();
            };
            return BackgroundImageData();
        }();

    if (d->backgroundType == type && d->imageData == data)
        return;

    d->backgroundType = type;
    d->imageData = data;
    d->imageStatus = ImageStatus::None;

    #ifdef NATIVE_PAINT_BACKGROUND
        m_imgAsFrame = QSharedPointer<CLVideoDecoderOutput>();
    #else
        d->image = QImage();
    #endif

    updateDisplay();
}

QRect QnGridBackgroundItem::sceneBoundingRect() const
{
    Q_D(const QnGridBackgroundItem);

    return !d->imageData.imageIsVisible(d->connected) ? QRect() : d->sceneBoundingRect;
}

QColor QnGridBackgroundItem::panelColor() const
{
    return m_panelColor;
}

void QnGridBackgroundItem::setPanelColor(const QColor& color)
{
    m_panelColor = color;
}

void QnGridBackgroundItem::updateGeometry()
{
    Q_D(QnGridBackgroundItem);

    if (d->backgroundType != BackgroundType::Image)
    {
        d->sceneBoundingRect = QRect();
        const qreal d = std::numeric_limits<qreal>::max() / 4;
        setViewportRect(QRectF(QPointF(-d, -d), QPointF(d, d)));
        return;
    }

    if(mapper() == NULL)
        return;

    d->sceneBoundingRect = QnLayoutResource::backgroundRect(d->imageData.size);
    const QRectF targetRect = mapper()->mapFromGrid(d->sceneBoundingRect);
    setViewportRect(targetRect);
}

void QnGridBackgroundItem::updateConnectedState()
{
    Q_D(QnGridBackgroundItem);
    d->connected = !commonModule()->remoteGUID().isNull();
    updateDisplay();
}

void QnGridBackgroundItem::at_imageLoaded(const QString& filename, bool ok)
{
    Q_D(QnGridBackgroundItem);

    if (filename != d->imageData.fileName || d->imageStatus != ImageStatus::Loading)
        return;

    d->imageStatus = ImageStatus::Loaded;
    if (!ok)
        return;

    if (d->imagesMemCache.contains(d->imageData.fileName))
    {
        setImage(d->imagesMemCache[d->imageData.fileName]);
        return;
    }

    const auto loader = new QnThreadedImageLoader(this);
    loader->setInput(cache()->getFullPath(filename));
    loader->setSize(cache()->getMaxImageSize());
    loader->setProperty(loaderFilenamePropertyName, d->imageData.fileName);

    #define QnThreadedImageLoaderFinished \
        static_cast<void (QnThreadedImageLoader::*)(const QImage& )> \
        (&QnThreadedImageLoader::finished)

    connect(loader, QnThreadedImageLoaderFinished, this, &QnGridBackgroundItem::setImage);
    loader->start();
}

ServerImageCache* QnGridBackgroundItem::cache()
{
    Q_D(const QnGridBackgroundItem);

    return d->imageData.isLocal
        ? context()->instance<LocalFileCache>()
        : context()->instance<ServerImageCache>();
}

void QnGridBackgroundItem::setImage(const QImage& image)
{
    Q_D(QnGridBackgroundItem);

    const auto filename = sender()
        ? sender()->property(loaderFilenamePropertyName).toString()
        : QString();

    if (!filename.isEmpty() && filename != d->imageData.fileName)
        return; // race condition

    if (d->imageStatus != ImageStatus::Loaded)    // image name was changed during load
        return;

    if (!d->imagesMemCache.contains(d->imageData.fileName))
        d->imagesMemCache.insert(d->imageData.fileName, image);

    d->imageAspectRatio = QnGeometry::aspectRatio(image.size());

#ifdef NATIVE_PAINT_BACKGROUND
    //converting image to ARGB32 since we cannot convert to YUV from monochrome, indexed, etc..
    const QImage* imgToLoad = NULL;
    std::unique_ptr<QImage> tempImgAp;
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
    // TODO: #ak it is possible to remove this copying and optimize loading by using ffmpeg to load picture files
    const unsigned int requiredImgXStride = qPower2Ceil( (unsigned int)imgToLoad->width()*4, X_STRIDE_FOR_SSE_CONVERT_UTILS );
    quint8* alignedImgBuffer = (quint8*)qMallocAligned( requiredImgXStride*imgToLoad->height(), X_STRIDE_FOR_SSE_CONVERT_UTILS );
    //copying image data to aligned buffer
    for( int y = 0; y < imgToLoad->height(); ++y )
        memcpy( alignedImgBuffer + requiredImgXStride*y, imgToLoad->constScanLine(y), imgToLoad->width()*imgToLoad->depth()/8 );

#ifdef USE_YUVA420
    m_imgAsFrame->reallocate( imgToLoad->width(), imgToLoad->height(), AV_PIX_FMT_YUVA420P );
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
    m_imgAsFrame->reallocate( imgToLoad->width(), imgToLoad->height(), AV_PIX_FMT_YUV420P );
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

void QnGridBackgroundItem::paint(
    QPainter* painter,
    const QStyleOptionGraphicsItem* /* option*/,
    QWidget* /*widget*/)
{
    Q_D(QnGridBackgroundItem);

    if (d->backgroundType == BackgroundType::Special)
    {
        painter->setBrush(m_panelColor);
        painter->drawRect(display()->boundedViewportGeometry(Qn::PanelMargins));
        return;
    }

    QRectF targetRect = d->rect;
    if (d->backgroundType == BackgroundType::Default)
    {
        switch (d->imageData.mode)
        {
            case Qn::ImageBehaviour::Fit:
                targetRect = QnGeometry::expanded(d->imageAspectRatio,
                    display()->viewportGeometry(), Qt::KeepAspectRatio, Qt::AlignCenter);
                break;
            case Qn::ImageBehaviour::Crop:
                targetRect = QnGeometry::expanded(d->imageAspectRatio,
                    display()->viewportGeometry(), Qt::KeepAspectRatioByExpanding, Qt::AlignCenter);
                break;
            default:
                targetRect = display()->viewportGeometry();
                break;
        }
    }

#ifdef NATIVE_PAINT_BACKGROUND
    if( !m_imgAsFrame )
        return;

    if(!m_imgUploader)
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

    if( m_imgAsFrame->format == AV_PIX_FMT_YUVA420P || m_imgAsFrame->format == AV_PIX_FMT_RGBA )
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    m_imgUploader->setOpacity( painter->opacity() );

    m_renderer->paint(QRectF(0, 0, 1, 1), targetRect);

    if( m_imgAsFrame->format == AV_PIX_FMT_YUVA420P || m_imgAsFrame->format == AV_PIX_FMT_RGBA )
        glDisable(GL_BLEND);

    QnGlNativePainting::end(painter);

#else
    if (!d->image.isNull())
        painter->drawImage(targetRect, d->image);
#endif
}
