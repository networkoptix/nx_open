#include "grid_background_item.h"

#include <QPainter>

#include <ui/animation/animation_timer.h>
#include <ui/animation/opacity_animator.h>
#include <ui/animation/rect_animator.h>
#include <ui/animation/variant_animator.h>

#include <ui/workbench/workbench_grid_mapper.h>

#include <utils/threaded_image_loader.h>

QnGridBackgroundItem::QnGridBackgroundItem(QGraphicsItem *parent):
    QGraphicsObject(parent),
    m_imageSize(1, 1),
    m_imageOpacity(70),
    m_targetOpacity(0),
    m_geometryAnimator(NULL),
    m_opacityAnimator(NULL),
    m_cache(new QnAppServerFileCache(this))
{
    setAcceptedMouseButtons(0);

    connect(m_cache, SIGNAL(imageLoaded(QString, bool)), this, SLOT(at_imageLoaded(QString, bool)));
    /* Don't disable this item here. When disabled, it starts accepting wheel events
     * (and probably other events too). Looks like a Qt bug. */
}

QRectF QnGridBackgroundItem::boundingRect() const {
    return m_rect;
}

QnGridBackgroundItem::~QnGridBackgroundItem() {
    return;
}

void QnGridBackgroundItem::updateDisplay() {
    if (m_imageFilename.isEmpty()) {
        animatedHide();
        return;
    }
    m_cache->loadImage(m_imageFilename);
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
    if (m_image.isNull())
        return;
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

QString QnGridBackgroundItem::imageFilename() const {
    return m_imageFilename;
}

void QnGridBackgroundItem::setImageFilename(const QString &imageFilename) {
    if (m_imageFilename == imageFilename)
        return;
    m_imageFilename = imageFilename;
    m_image = QImage();
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
//    if (m_geometryAnimator)
//        m_geometryAnimator->animateTo(targetRect);
//    else
        setViewportRect(targetRect);
}

void QnGridBackgroundItem::at_opacityAnimator_finished() {
    if (!qFuzzyCompare(this->opacity(), m_targetOpacity))
         m_opacityAnimator->animateTo(m_targetOpacity);
}

void QnGridBackgroundItem::at_imageLoaded(const QString& filename, bool ok) {
    if (!ok || filename != m_imageFilename)
        return;

    QnThreadedImageLoader* loader = new QnThreadedImageLoader(this);
    loader->setInput(m_cache->getFullPath(filename));
    loader->setSize(m_cache->getMaxImageSize());
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

