#include "grid_background_item.h"

#include <QPainter>

#include <ui/animation/animation_timer.h>
#include <ui/animation/opacity_animator.h>
#include <ui/animation/rect_animator.h>
#include <ui/animation/variant_animator.h>

#include <ui/graphics/opengl/gl_functions.h>

#include <ui/workbench/workbench_grid_mapper.h>

#include <utils/threaded_image_loader.h>

QnGridBackgroundItem::QnGridBackgroundItem(QGraphicsItem *parent):
    QGraphicsObject(parent),
    m_imageId(0),
    m_imageSize(1, 1),
    m_imageOpacity(70),
    m_targetOpacity(0),
    m_geometryAnimator(NULL),
    m_opacityAnimator(NULL),
    m_cache(new QnAppServerFileCache(this))
{
    setAcceptedMouseButtons(0);

    connect(m_cache, SIGNAL(imageLoaded(int)), this, SLOT(at_image_loaded(int)));
    /* Don't disable this item here. When disabled, it starts accepting wheel events
     * (and probably other events too). Looks like a Qt bug. */
}

QRectF QnGridBackgroundItem::boundingRect() const {
    return m_rect;
}

QnGridBackgroundItem::~QnGridBackgroundItem() {
    return;
}

void QnGridBackgroundItem::showWhenReady() {
    if (m_imageId == 0)
        return;
    m_cache->loadImage(m_imageId);
}

void QnGridBackgroundItem::animatedHide() {
    /*
    m_targetOpacity = 0.0;
    if (!m_opacityAnimator)
        return;

    if (!m_opacityAnimator->isRunning())
        m_opacityAnimator->animateTo(m_targetOpacity);
    */
    setOpacity(0.0);
}

void QnGridBackgroundItem::animatedShow() {
    /*
    m_targetOpacity = 0.7;
    if (!m_opacityAnimator)
        return;

    if (!m_opacityAnimator->isRunning())
        m_opacityAnimator->animateTo(m_targetOpacity);
    */
    setOpacity(0.01 * m_imageOpacity);
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


AnimationTimer* QnGridBackgroundItem::animationTimer() const {
    return m_geometryAnimator->timer();
}

void QnGridBackgroundItem::setAnimationTimer(AnimationTimer *timer) {
    if (!m_geometryAnimator) {
        m_geometryAnimator = new RectAnimator(this);
        m_geometryAnimator->setTargetObject(this);
        m_geometryAnimator->setAccessor(new PropertyAccessor("viewportRect"));
    }

    if (!m_opacityAnimator) {
        m_opacityAnimator = opacityAnimator(this, 0.5);
        m_opacityAnimator->setTimeLimit(2500);
        connect(m_opacityAnimator, SIGNAL(finished()), this, SLOT(at_opacityAnimator_finished()));
    }

    m_geometryAnimator->setTimer(timer);
}

int QnGridBackgroundItem::imageId() const {
    return m_imageId;
}

void QnGridBackgroundItem::setImageId(int imageId) {
    m_imageId = imageId;
}

QSize QnGridBackgroundItem::imageSize() const {
    return m_imageSize;
}

void QnGridBackgroundItem::setImageSize(const QSize &imageSize) {
    if (m_imageSize == imageSize)
        return;
    m_imageSize = imageSize;
    updateGeometry();
}

int QnGridBackgroundItem::imageOpacity() const {
    return m_imageOpacity;
}

void QnGridBackgroundItem::setImageOpacity(int percent) {
    m_imageOpacity = percent;
}

QRect QnGridBackgroundItem::sceneBoundingRect() const {
    if (m_imageId == 0)
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
//    if (m_geometryAnimator)
//        m_geometryAnimator->animateTo(targetRect);
//    else
        setViewportRect(targetRect);
}

void QnGridBackgroundItem::at_opacityAnimator_finished() {
    if (!qFuzzyCompare(this->opacity(), m_targetOpacity))
         m_opacityAnimator->animateTo(m_targetOpacity);
}

void QnGridBackgroundItem::at_image_loaded(int id) {
    if (id != m_imageId)
        return;

  //  int maxTextureSize = QnGlFunctions::estimatedInteger(GL_MAX_TEXTURE_SIZE);
    int maxTextureSize = 4096;
    //int maxTextureSize = 6144;

    QnThreadedImageLoader* loader = new QnThreadedImageLoader(this);
    loader->setInput(m_cache->getPath(id));
    loader->setSize(QSize(maxTextureSize, maxTextureSize));
    //loader->setAspectRatioMode(Qt::IgnoreAspectRatio);
    //loader->setDownScaleOnly(false);
    connect(loader, SIGNAL(finished(QImage)), this, SLOT(setImage(QImage)));
    loader->start();
}

void QnGridBackgroundItem::setImage(const QImage &image) {
    m_image = image;
    animatedShow();
}

void QnGridBackgroundItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) {
    if (!m_image.isNull())
        painter->drawImage(m_rect, m_image);
}

