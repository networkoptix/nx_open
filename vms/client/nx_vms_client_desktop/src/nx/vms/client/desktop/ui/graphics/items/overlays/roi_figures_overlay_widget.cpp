// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "roi_figures_overlay_widget.h"

#include <QtGui/QTextCharFormat>
#include <QtGui/QAbstractTextDocumentLayout>
#include <QtGui/QTextDocument>
#include <QtGui/QTextCursor>
#include <QtGui/QPainter>
#include <QtOpenGLWidgets/QOpenGLWidget>
#include <QtWidgets/QGraphicsTextItem>

#include <nx/vms/client/desktop/ui/graphics/items/overlays/figure/decorations_helper.h>
#include <nx/vms/client/desktop/ui/graphics/items/overlays/figure/figure_item.h>
#include <nx/vms/client/desktop/ui/graphics/items/overlays/figure/figure.h>
#include <nx/vms/client/desktop/ui/graphics/items/overlays/figure/renderer.h>
#include <nx/vms/client/desktop/ui/graphics/items/overlays/figure/figures_watcher.h>

namespace {

static constexpr int kRoiLineWidth = 2;
static constexpr bool kFillRoiClosedShapes = true;

} // namespace

namespace nx::vms::client::desktop {

using namespace figure;

struct RoiFiguresOverlayWidget::Private: public QObject
{
    using FigureItemPtr = QSharedPointer<FigureItem>;
    using FigureItemsHash = QHash<FigureId, FigureItemPtr>;
    using FigureItemsByEngine = QHash<EngineId, FigureItemsHash>;

    RoiFiguresOverlayWidget* const q;

    QRectF zoomRect;
    FigureItemsByEngine roiItems;
    RoiFiguresWatcher watcher;
    const RendererPtr renderer;

    Private(
        const QnVirtualCameraResourcePtr& camera,
        RoiFiguresOverlayWidget* q);

    void addOrUpdateRoi(
        const EngineId& engineId,
        const FigureId& figureId);

    void removeRoi(
        const EngineId& engineId,
        const FigureId& figureId);

    void updateRoiItemPosition(const FigureItemPtr& roiItem);

    void updateRoiItemPositions();

    void updateFigures();

    void paintFigureLabels(
        QPainter* painter,
        const QSizeF& widgetSize);

    void paintLabel(
        QPainter* painter,
        FigurePtr figure,
        const QString& labelText,
        const QSizeF& widgetSize);
};

RoiFiguresOverlayWidget::Private::Private(
    const QnVirtualCameraResourcePtr& camera,
    RoiFiguresOverlayWidget* q)
    :
    q(q),
    watcher(camera),
    renderer(new figure::Renderer(kRoiLineWidth, kFillRoiClosedShapes))
{
    connect(&watcher, &RoiFiguresWatcher::figureAdded, this, &Private::addOrUpdateRoi);
    connect(&watcher, &RoiFiguresWatcher::figureChanged, this, &Private::addOrUpdateRoi);
    connect(&watcher, &RoiFiguresWatcher::figureRemoved, this, &Private::removeRoi);

    updateFigures();
}

void RoiFiguresOverlayWidget::Private::addOrUpdateRoi(
    const EngineId& engineId,
    const FigureId& figureId)
{
    auto figure = watcher.figure(engineId, figureId);

    if (figure && zoomRect.isValid())
    {
        if (figure->intersects(zoomRect))
            figure->setSceneRect(zoomRect);
        else
            figure.reset();
    }

    if (!figure)
    {
        removeRoi(engineId, figureId);
        return;
    }

    FigureItemPtr roiItem;
    const auto itItemsByEngine = roiItems.find(engineId);
    if (itItemsByEngine == roiItems.end())
    {
        roiItem.reset(new FigureItem(figure, renderer, q));
        roiItems.insert(engineId, {{figureId, roiItem}});
    }
    else
    {
        const auto itItem = itItemsByEngine->find(figureId);
        if (itItem == itItemsByEngine->end())
        {
            roiItem.reset(new FigureItem(figure, renderer, q));
            itItemsByEngine->insert(figureId, roiItem);
        }
        else
        {
            roiItem = itItem.value();
            roiItem->setFigure(figure);
        }
    }

    updateRoiItemPosition(roiItem);
}

void RoiFiguresOverlayWidget::Private::removeRoi(
    const EngineId& engineId,
    const FigureId& figureId)
{
    const auto itItemsByEngine = roiItems.find(engineId);
    if (itItemsByEngine == roiItems.end())
        return;

    const auto itItem = itItemsByEngine->find(figureId);
    if (itItem == itItemsByEngine->end())
        return;

    itItemsByEngine->erase(itItem);
    if (itItemsByEngine->empty())
        roiItems.erase(itItemsByEngine);
}

void RoiFiguresOverlayWidget::Private::updateRoiItemPosition(const FigureItemPtr& roiItem)
{
    if (!roiItem)
        return;

    const auto figure = roiItem->figure();
    if (figure)
        roiItem->setPos(figure->pos(q->size()));
}

void RoiFiguresOverlayWidget::Private::updateRoiItemPositions()
{
    for (const auto& itemsByEngine: roiItems)
    {
        for (const auto& roiItem: itemsByEngine)
            updateRoiItemPosition(roiItem);
    }
}

void RoiFiguresOverlayWidget::Private::updateFigures()
{
    const auto figuresHolder = watcher.figures();
    for (auto itEngine = figuresHolder.cbegin(); itEngine != figuresHolder.end(); ++itEngine)
    {
        const auto engineId = itEngine.key();
        for (const auto& figureId: itEngine.value().keys())
            addOrUpdateRoi(engineId, figureId);
    }
}

void RoiFiguresOverlayWidget::Private::paintFigureLabels(
    QPainter* painter,
    const QSizeF& widgetSize)
{
    const auto figuresHolder = watcher.figures();
    for (auto itEngine = figuresHolder.cbegin(); itEngine != figuresHolder.end(); ++itEngine)
    {
        const auto engineId = itEngine.key();
        for (const auto& figureId: itEngine.value().keys())
        {
            const FigurePtr figure = watcher.figure(engineId, figureId);
            const QString labelText = watcher.figureLabelText(engineId, figureId);
            if (figure && zoomRect.isValid())
            {
                if (!figure->intersects(zoomRect))
                    continue;

                figure->setSceneRect(zoomRect);
            }

            paintLabel(painter, figure, labelText, widgetSize);
        }
    }
}

void RoiFiguresOverlayWidget::Private::paintLabel(
    QPainter* painter,
    FigurePtr figure,
    const QString& labelText,
    const QSizeF& widgetSize)
{
    static constexpr qreal kTopMargin = 3;
    static constexpr qreal kBottomMargin = 4;
    static const QFont kFigureLabelFont("Roboto", 12, QFont::Medium);

    QTextDocument doc(labelText.toUpper());
    doc.setDocumentMargin(0);

    const auto labelPos = getLabelPosition(
        widgetSize,
        figure,
        doc.size(),
        kTopMargin,
        kBottomMargin);

    if (!labelPos.has_value())
        return;

    painter->save();
    painter->setClipRect({{0, 0}, widgetSize});
    QTransform transform = painter->transform();
    transform.translate(labelPos.value().coordinates.x(), labelPos.value().coordinates.y());
    transform.rotate(labelPos.value().angle);
    painter->setTransform(transform);

    QTextCharFormat format;
    QFont font = kFigureLabelFont;
    font.setLetterSpacing(QFont::AbsoluteSpacing, 0.5);
    format.setFont(font);
    // Color and width of outline.
    format.setTextOutline(QPen(Qt::black, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));

    QAbstractTextDocumentLayout::PaintContext ctx;
    ctx.palette.setColor(QPalette::Text, figure->color());

    QTextCursor cursor(&doc);
    cursor.select(QTextCursor::Document);
    cursor.mergeCharFormat(format);
    doc.documentLayout()->draw(painter, ctx);

    format.setTextOutline(QPen(Qt::transparent));
    cursor.mergeCharFormat(format);
    doc.documentLayout()->draw(painter, ctx);

    painter->restore();
}

//-------------------------------------------------------------------------------------------------

RoiFiguresOverlayWidget::RoiFiguresOverlayWidget(
    const QnVirtualCameraResourcePtr& camera,
    QGraphicsItem* parent)
    :
    base_type(parent),
    d(new Private(camera, this))
{
    setAcceptedMouseButtons(Qt::NoButton);
    setFocusPolicy(Qt::NoFocus);

    connect(this, &QGraphicsWidget::geometryChanged, this, [this]() { d->updateRoiItemPositions(); });
}

RoiFiguresOverlayWidget::~RoiFiguresOverlayWidget()
{
}

void RoiFiguresOverlayWidget::setZoomRect(const QRectF& value)
{
    if (d->zoomRect == value)
        return;

    d->zoomRect = value;
    d->updateFigures();
}

void RoiFiguresOverlayWidget::paint(
    QPainter* painter,
    const QStyleOptionGraphicsItem* /*option*/,
    QWidget* /*widget*/)
{
    d->paintFigureLabels(painter, size());
}

} // namespace nx::vms::client::desktop
