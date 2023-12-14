// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "grid_background_item.h"

#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLFunctions>
#include <QtGui/QPainter>

#include <client_core/client_core_module.h>
#include <common/common_module.h>
#include <core/resource/user_resource.h>
#include <nx/utils/math/math.h>
#include <nx/vms/client/core/network/network_module.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/core/network/remote_session.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/utils/geometry.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/image_providers/threaded_image_loader.h>
#include <nx/vms/client/desktop/resource/layout_resource.h>
#include <nx/vms/client/desktop/settings/local_settings.h>
#include <nx/vms/client/desktop/utils/local_file_cache.h>
#include <nx/vms/client/desktop/utils/server_image_cache.h>
#include <ui/workaround/gl_native_painting.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_grid_mapper.h>
#include <ui/workbench/workbench_layout.h>
#include <utils/color_space/yuvconvert.h>

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

using namespace nx::vms::client::desktop;
using nx::vms::client::core::Geometry;

namespace {

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

BackgroundType backgroundTypeFromLayout(const LayoutResourcePtr& layout)
{
    if (!layout->hasBackground())
    {
        const auto workbenchLayout = QnWorkbenchLayout::instance(layout);
        if (workbenchLayout && workbenchLayout->flags().testFlag(QnLayoutFlag::SpecialBackground))
            return BackgroundType::Special;

        return BackgroundType::Default;
    }

    return BackgroundType::Image;
}

//////////////////////////////////////////////////////////////////////////////////////////////////

static const QSize kDefaultImageSize = QSize(1, 1);
struct BackgroundImageData
{
    QString fileName;
    QSize size = kDefaultImageSize;
    qreal opacity = 1;
    Qn::ImageBehavior mode = Qn::ImageBehavior::Stretch;
    bool isLocal = true;

    BackgroundImageData() = default;

    BackgroundImageData(
        const QString& fileName,
        const QSize& size,
        qreal opacity,
        Qn::ImageBehavior mode,
        bool isLocal);

    static BackgroundImageData getForDefaultType();
    static BackgroundImageData getForImageType(const QnLayoutResourcePtr& layout);

    bool imageIsVisible(bool connected) const;

    bool operator==(const BackgroundImageData& other) const;

    [[maybe_unused]]
    bool operator!=(const BackgroundImageData& other) const;
};

BackgroundImageData::BackgroundImageData(
    const QString& fileName,
    const QSize& size,
    qreal opacity,
    Qn::ImageBehavior mode,
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
    const BackgroundImage background = appContext()->localSettings()->backgroundImage();
    return BackgroundImageData(
        background.name,
        kDefaultImageSize,
        background.actualImageOpacity(),
        background.mode,
        true); //< Image is always local for default background image.
}

BackgroundImageData BackgroundImageData::getForImageType(const QnLayoutResourcePtr& layout)
{
    const BackgroundImage background = appContext()->localSettings()->backgroundImage();
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
    QHash<QString, QImage> imagesMemCache;  // TODO: #sivanov Replace with QCache.

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
    QnWorkbenchContextAware(context),
    d_ptr(new QnGridBackgroundItemPrivate()),
    m_panelColor(nx::vms::client::core::colorTheme()->color("scene.customLayoutBackground"))
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

    connect(context,
        &QnWorkbenchContext::userChanged,
        this,
        &QnGridBackgroundItem::updateConnectedState);

    connect(&appContext()->localSettings()->backgroundImage,
        &nx::utils::property_storage::BaseProperty::changed,
        this,
        &QnGridBackgroundItem::updateDefaultBackground);

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

    const BackgroundImage background = appContext()->localSettings()->backgroundImage();

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

void QnGridBackgroundItem::update(const LayoutResourcePtr& layout)
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

    updateConnectedState();
}

QRect QnGridBackgroundItem::sceneBoundingRect() const
{
    Q_D(const QnGridBackgroundItem);

    return !d->imageData.imageIsVisible(d->connected) ? QRect() : d->sceneBoundingRect;
}

void QnGridBackgroundItem::updateGeometry()
{
    Q_D(QnGridBackgroundItem);

    if (d->backgroundType != BackgroundType::Image)
    {
        d->sceneBoundingRect = QRect();
        setViewportRect(Geometry::maxBoundingRect());
        return;
    }

    if (!mapper())
        return;

    d->sceneBoundingRect = QnLayoutResource::backgroundRect(d->imageData.size);
    const QRectF targetRect = mapper()->mapFromGrid(d->sceneBoundingRect);
    setViewportRect(targetRect);
}

void QnGridBackgroundItem::updateConnectedState()
{
    Q_D(QnGridBackgroundItem);

    d->connected = !context()->currentServer().isNull();
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

    const auto loader = new ThreadedImageLoader(this);
    loader->setInput(cache()->getFullPath(filename));
    loader->setSize(cache()->getMaxImageSize());
    connect(loader, &ThreadedImageLoader::imageLoaded, this,
        [this, filename = d->imageData.fileName](const QImage& image)
        {
            setImage(image, filename);
        });
    connect(loader, &ThreadedImageLoader::imageLoaded, loader, &QObject::deleteLater);
    loader->start();
}

ServerImageCache* QnGridBackgroundItem::cache()
{
    Q_D(const QnGridBackgroundItem);

    return d->imageData.isLocal
        ? context()->instance<LocalFileCache>()
        : context()->instance<ServerImageCache>();
}

void QnGridBackgroundItem::setImage(const QImage& image, const QString& filename)
{
    Q_D(QnGridBackgroundItem);

    if (!filename.isEmpty() && filename != d->imageData.fileName)
        return; // race condition

    if (d->imageStatus != ImageStatus::Loaded)    // image name was changed during load
        return;

    if (!d->imagesMemCache.contains(d->imageData.fileName))
        d->imagesMemCache.insert(d->imageData.fileName, image);

    d->imageAspectRatio = Geometry::aspectRatio(image.size(), 0.0);

#ifdef NATIVE_PAINT_BACKGROUND
    //converting image to ARGB32 since we cannot convert to YUV from monochrome, indexed, etc..
    const QImage* imgToLoad = nullptr;
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

    //adding stride to source data
    // TODO: #akolesnikov it is possible to remove this copying and optimize loading by using ffmpeg to load picture files
    const unsigned int requiredImgXStride = qPower2Ceil( (unsigned int)imgToLoad->width()*4, X_STRIDE_FOR_SSE_CONVERT_UTILS );
    quint8* alignedImgBuffer = (quint8*)qMallocAligned( requiredImgXStride*imgToLoad->height(), X_STRIDE_FOR_SSE_CONVERT_UTILS );

    // Check if there is enough memory to preprocess the image.
    if (!alignedImgBuffer)
        return;

    //copying image data to aligned buffer
    for( int y = 0; y < imgToLoad->height(); ++y )
        memcpy( alignedImgBuffer + requiredImgXStride*y, imgToLoad->constScanLine(y), imgToLoad->width()*imgToLoad->depth()/8 );

    m_imgAsFrame = QSharedPointer<CLVideoDecoderOutput>( new CLVideoDecoderOutput() );

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
    QWidget* widget)
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
            case Qn::ImageBehavior::Fit:
                targetRect = Geometry::expanded(d->imageAspectRatio,
                    display()->viewportGeometry(), Qt::KeepAspectRatio, Qt::AlignCenter);
                break;
            case Qn::ImageBehavior::Crop:
                targetRect = Geometry::expanded(d->imageAspectRatio,
                    display()->viewportGeometry(), Qt::KeepAspectRatioByExpanding, Qt::AlignCenter);
                break;
            default:
                targetRect = display()->viewportGeometry();
                break;
        }
    }

#ifdef NATIVE_PAINT_BACKGROUND

    const auto glWidget = qobject_cast<QOpenGLWidget*>(widget);
    if (!glWidget)
        return;

    if (!m_imgAsFrame)
        return;

    if (!m_imgUploader)
    {
        m_imgUploader.reset(new DecodedPictureToOpenGLUploader(glWidget, nullptr));
        m_renderer.reset(new QnGLRenderer(glWidget, *m_imgUploader));
        m_imgUploader->setYV12ToRgbShaderUsed(m_renderer->isYV12ToRgbShaderUsed());
        m_imgUploader->setNV12ToRgbShaderUsed(m_renderer->isNV12ToRgbShaderUsed());
    }

    if (!d->imgUploaded)
    {
        //uploading image to opengl texture
        m_imgUploader->uploadDecodedPicture( m_imgAsFrame );
        d->imgUploaded = true;
    }

    QnGlNativePainting::begin(glWidget, painter);
    const auto functions = glWidget->context()->functions();

    if (m_imgAsFrame->format == AV_PIX_FMT_YUVA420P || m_imgAsFrame->format == AV_PIX_FMT_RGBA)
    {
        functions->glEnable(GL_BLEND);
        functions->glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    m_imgUploader->setOpacity(painter->opacity());

    m_renderer->paint(nullptr, QRectF(0, 0, 1, 1), targetRect);

    if (m_imgAsFrame->format == AV_PIX_FMT_YUVA420P || m_imgAsFrame->format == AV_PIX_FMT_RGBA)
        functions->glDisable(GL_BLEND);

    QnGlNativePainting::end(painter);

#else
    if (!d->image.isNull())
        painter->drawImage(targetRect, d->image);
#endif
}
