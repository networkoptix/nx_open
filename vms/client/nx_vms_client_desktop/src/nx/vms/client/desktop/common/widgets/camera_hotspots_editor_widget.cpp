// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_hotspots_editor_widget.h"

#include <QtGui/QPainter>
#include <QtGui/QPainterPath>
#include <QtGui/QWheelEvent>
#include <QtWidgets/QLayout>
#include <QtWidgets/QMenu>

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/math/math.h>
#include <nx/vms/client/core/access/access_controller.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/utils/geometry.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/camera_hotspots/camera_hotspots_display_utils.h>
#include <nx/vms/client/desktop/common/utils/widget_anchor.h>
#include <nx/vms/client/desktop/common/widgets/busy_indicator.h>
#include <nx/vms/client/desktop/resource_properties/camera/widgets/camera_hotspots_item_delegate.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/thumbnails/live_camera_thumbnail.h>
#include <ui/common/palette.h>
#include <ui/workaround/hidpi_workarounds.h>
#include <utils/common/scoped_painter_rollback.h>

using Geometry = nx::vms::client::core::Geometry;

namespace {

QFont noDataPlaceholderFont()
{
    static constexpr auto kFontPixelSize = 24;
    static constexpr auto kFontWeight = QFont::Normal;

    QFont font;
    font.setCapitalization(QFont::AllUppercase);
    font.setPixelSize(kFontPixelSize);
    font.setWeight(kFontWeight);
    return font;
}

QFont noAccessPlaceholderFont()
{
    static constexpr auto kFontPixelSize = 32;
    static constexpr auto kFontWeight = QFont::Normal;

    QFont font;
    font.setCapitalization(QFont::AllUppercase);
    font.setPixelSize(kFontPixelSize);
    font.setWeight(kFontWeight);
    return font;
}

// Intersection points of a ray defined by origin and direction and a circle defined by the center
// point and radius.
QVector<QPointF> rayAndCircleIntersectionPoints(
    const QPointF& rayOrigin,
    const QPointF& rayDirection,
    const QPointF& circleCenter,
    double circleRadius)
{
    if (!NX_ASSERT(!qFuzzyIsNull(Geometry::length(rayDirection))))
        return {};

    if (!NX_ASSERT(circleRadius >= 0.0))
        return {};

    const auto normalizedDirection = Geometry::normalized(rayDirection);
    QPointF deltaVector = circleCenter - rayOrigin;

    double distanceToChordCenter = QPointF::dotProduct(normalizedDirection, deltaVector);
    double halfChordLengthSquared =
        std::pow(distanceToChordCenter, 2.0)
        - Geometry::lengthSquared(deltaVector)
        + std::pow(circleRadius, 2.0);

    QVector<QPointF> result;

    if (qFuzzyIsNull(halfChordLengthSquared)) //< Tangent.
    {
        if (qFuzzyIsNull(distanceToChordCenter) || distanceToChordCenter > 0)
            result.push_back(rayOrigin + normalizedDirection * distanceToChordCenter);
    }
    else if (halfChordLengthSquared > 0)
    {
        const auto halfChordLength = std::sqrt(halfChordLengthSquared);

        const auto distanceToIntersection1 = distanceToChordCenter - halfChordLength;
        const auto distanceToIntersection2 = distanceToChordCenter + halfChordLength;

        if (qFuzzyIsNull(distanceToIntersection1) || distanceToIntersection1 > 0)
            result.push_back(rayOrigin + normalizedDirection * distanceToIntersection1);

        if (qFuzzyIsNull(distanceToIntersection2) || distanceToIntersection2 > 0)
            result.push_back(rayOrigin + normalizedDirection * distanceToIntersection2);
    }

    return result;
}

} // namespace

namespace nx::vms::client::desktop {

using CameraHotspotData = nx::vms::common::CameraHotspotData;
using CameraHotspotDataList = nx::vms::common::CameraHotspotDataList;

//-------------------------------------------------------------------------------------------------
// CameraHotspotEditorWidget::Private declaration.

struct CameraHotspotsEditorWidget::Private
{
    CameraHotspotsEditorWidget* const q;

    QSharedPointer<LiveCameraThumbnail> cameraThumbnail;
    BusyIndicatorWidget* loadingIndicator = nullptr;

    CameraHotspotDataList hotspots;
    std::optional<int> selectedHotspotIndex;
    std::optional<int> hoveredHotspotIndex;

    enum class DragAction
    {
        moveOrigin,
        setDirection,
    };
    std::optional<DragAction> dragAction;
    bool hasModificationsByDrag = false;

    QRect thumbnailRect() const;

    bool isSelectedHotspotIndex(int index) const;
    void setSelectedHotspotIndex(std::optional<int> index);

    bool isHoveredHotspotIndex(int index) const;
    void setHoveredHotspotIndex(std::optional<int> index);

    std::optional<int> hotspotIndexAtPos(const QPoint& pos) const;

    void processContextMenuRequest(const QPoint& pos);

    QPoint resolveHotspotsCollision(
        const QPoint& desiredPosition,
        std::optional<int> modifiedHotspotIndex = std::nullopt) const;
};

//-------------------------------------------------------------------------------------------------
// CameraHotspotEditorWidget::Private definition.

QRect CameraHotspotsEditorWidget::Private::thumbnailRect() const
{
    using namespace nx::vms::client::core;

    return Geometry::aligned(
        Geometry::expanded(QSizeF(cameraThumbnail->aspectRatio(), 1.0), q->size()),
            q->rect()).toRect();
}

bool CameraHotspotsEditorWidget::Private::isSelectedHotspotIndex(int index) const
{
    return selectedHotspotIndex && selectedHotspotIndex.value() == index;
}

void CameraHotspotsEditorWidget::Private::setSelectedHotspotIndex(std::optional<int> index)
{
    if (selectedHotspotIndex == index)
        return;

    dragAction.reset();

    selectedHotspotIndex = index;
    q->update();
    q->selectedHotspotChanged();
}

bool CameraHotspotsEditorWidget::Private::isHoveredHotspotIndex(int index) const
{
    return hoveredHotspotIndex && hoveredHotspotIndex.value() == index;
}

void CameraHotspotsEditorWidget::Private::setHoveredHotspotIndex(std::optional<int> index)
{
    if (hoveredHotspotIndex == index)
        return;

    hoveredHotspotIndex = index;
    q->update();
}

std::optional<int> CameraHotspotsEditorWidget::Private::hotspotIndexAtPos(const QPoint& pos) const
{
    for (int i = 0; i < hotspots.size(); ++i)
    {
        if (camera_hotspots::makeHotspotOutline(hotspots.at(i), thumbnailRect()).contains(pos))
            return i;
    }
    return std::nullopt;
}

void CameraHotspotsEditorWidget::Private::processContextMenuRequest(const QPoint& pos)
{
    if (!thumbnailRect().contains(pos))
        return;

    const auto menu = new QMenu(q);
    connect(menu, &QMenu::aboutToHide, menu, &QMenu::deleteLater);

    if (const auto hotspotIndex = hotspotIndexAtPos(pos))
    {
        // Delete action.
        const auto deleteAction = menu->addAction(tr("Delete"));
        QObject::connect(deleteAction, &QAction::triggered, q,
            [this, index = hotspotIndex.value()] { q->removeHotstpotAt(index); });

        // Select camera action.
        const auto editAction = menu->addAction(tr("Select Camera..."));
        QObject::connect(editAction, &QAction::triggered, q,
            [this, index = hotspotIndex.value()] { emit q->selectHotspotCamera(index); });

        // Toggle pointed hotspot shape action.
        const auto pointedShapeToggleAction = menu->addAction(tr("Pointed"));
        const auto hasPointedShape =
            !qFuzzyIsNull(Geometry::length(q->hotspotAt(hotspotIndex.value()).direction));
        pointedShapeToggleAction->setCheckable(true);
        pointedShapeToggleAction->setChecked(hasPointedShape);

        QObject::connect(pointedShapeToggleAction, &QAction::triggered, q,
            [this, hasPointedShape, index = hotspotIndex.value()]
            {
                auto hotspotData = q->hotspotAt(index);
                if (hasPointedShape)
                {
                    hotspotData.direction = QPointF();
                }
                else
                {
                    hotspotData.direction = qFuzzyEquals(hotspotData.pos, QPointF(0.5, 0.5))
                        ? QPointF(0.0, -1.0)
                        : Geometry::normalized(hotspotData.pos - QPointF(0.5, 0.5));
                }
                q->setHotspotAt(hotspotData, index);
            });
    }
    else
    {
        const auto createAction = menu->addAction(tr("Place Hotspot"));
        QObject::connect(createAction, &QAction::triggered, q,
            [this, pos]
            {
                static const auto kHotspotsPalette = CameraHotspotsItemDelegate::hotspotsPalette();

                CameraHotspotData hotspot;
                if (NX_ASSERT(!kHotspotsPalette.empty()))
                    hotspot.accentColorName = kHotspotsPalette.first().name();
                camera_hotspots::setHotspotPositionFromPointInRect(thumbnailRect(), pos, hotspot);
                q->appendHotspot(hotspot);
            });
    }

    QnHiDpiWorkarounds::showMenu(menu, QCursor::pos());
}

QPoint CameraHotspotsEditorWidget::Private::resolveHotspotsCollision(
    const QPoint& desiredPosition,
    std::optional<int> modifiedHotspotIndex) const
{
    QVector<QPointF> hotspotsOrigins;
    for (int i = 0; i < hotspots.size(); ++i)
    {
        if (i == modifiedHotspotIndex)
            continue;

        hotspotsOrigins.push_back(
            camera_hotspots::hotspotOrigin(hotspots.at(i), thumbnailRect()));
    }

    auto isAcceptablePosition =
        [hotspotsOrigins](const QPointF& pos)
        {
            return std::all_of(hotspotsOrigins.begin(), hotspotsOrigins.end(),
                [pos](const auto& origin)
                {
                    static constexpr auto kTolerance = 0.01;
                    return camera_hotspots::kHotspotRadius * 2 - Geometry::length(origin - pos)
                        < kTolerance;
                });
        };

    if (isAcceptablePosition(desiredPosition))
        return desiredPosition;

    QVector<QPointF> possiblePositions;
    for (int angle = 0; angle < 360; angle += 5)
    {
        const QPointF offsetDirection = Geometry::rotated(QPointF(0, 1), QPointF(0, 0), angle);
        for (const auto& origin: hotspotsOrigins)
        {
            possiblePositions += rayAndCircleIntersectionPoints(
                desiredPosition, offsetDirection, origin, camera_hotspots::kHotspotRadius * 2);
        }
    }

    std::sort(possiblePositions.begin(), possiblePositions.end(),
        [desiredPosition](const QPointF& lhs, const QPointF& rhs)
        {
            return Geometry::length(desiredPosition - lhs)
                < Geometry::length(desiredPosition - rhs);
        });

    for (const auto& possiblePosition: possiblePositions)
    {
        if (isAcceptablePosition(possiblePosition))
            return possiblePosition.toPoint();
    }

    return desiredPosition;
}

//-------------------------------------------------------------------------------------------------
// CameraHotspotEditorWidget definition.

CameraHotspotsEditorWidget::CameraHotspotsEditorWidget(QWidget* parent):
    base_type(parent),
    d(new Private{this})
{
    setMouseTracking(true);
    setContextMenuPolicy(Qt::CustomContextMenu);

    setPaletteColor(this, QPalette::Window, core::colorTheme()->color("dark5"));
    setBackgroundRole(QPalette::Window);
    setAutoFillBackground(true);

    d->loadingIndicator = new BusyIndicatorWidget(this);
    anchorWidgetToParent(d->loadingIndicator);

    connect(this, &CameraHotspotsEditorWidget::customContextMenuRequested, this,
        [this](const QPoint& pos)
        {
            d->processContextMenuRequest(pos);
        });
}

CameraHotspotsEditorWidget::~CameraHotspotsEditorWidget()
{
}

void CameraHotspotsEditorWidget::setThumbnail(const QSharedPointer<LiveCameraThumbnail>& thumbnail)
{
    d->cameraThumbnail = thumbnail;

    connect(thumbnail.get(), &LiveCameraThumbnail::statusChanged, this,
        [this]
        {
            const auto status = d->cameraThumbnail->status();
            d->loadingIndicator->setVisible(status == LiveCameraThumbnail::Status::loading);
            updateGeometry();
            update();
        });
}

bool CameraHotspotsEditorWidget::hasHeightForWidth() const
{
    return true;
}

int CameraHotspotsEditorWidget::heightForWidth(int width) const
{
    return qRound(width / d->cameraThumbnail->aspectRatio());
}

CameraHotspotDataList CameraHotspotsEditorWidget::getHotspots() const
{
    return d->hotspots;
}

void CameraHotspotsEditorWidget::setHotspots(const CameraHotspotDataList& hotspots)
{
    if (d->hotspots == hotspots)
        return;

    if (d->hotspots.size() != hotspots.size())
        d->setSelectedHotspotIndex(std::nullopt);

    d->hotspots = hotspots;
    update();
    emit hotspotsDataChanged();
}

int CameraHotspotsEditorWidget::hotspotsCount() const
{
    return d->hotspots.size();
}

CameraHotspotData CameraHotspotsEditorWidget::hotspotAt(int index)
{
    return d->hotspots.at(index);
}

void CameraHotspotsEditorWidget::setHotspotAt(const CameraHotspotData& hotspotData, int index)
{
    if (d->hotspots.at(index) == hotspotData)
        return;

    d->hotspots[index] = hotspotData;
    update();
    emit hotspotsDataChanged();
}

int CameraHotspotsEditorWidget::appendHotspot(const CameraHotspotData& hotspotData)
{
    auto fixedHotspotData = hotspotData;
    const auto desiredHotspotOrigin =
        camera_hotspots::hotspotOrigin(hotspotData, d->thumbnailRect());

    camera_hotspots::setHotspotPositionFromPointInRect(
        d->thumbnailRect(),
        d->resolveHotspotsCollision(desiredHotspotOrigin.toPoint()),
        fixedHotspotData);

    d->hotspots.push_back(fixedHotspotData);
    update();
    emit hotspotsDataChanged();
    return d->hotspots.size() - 1;
}

void CameraHotspotsEditorWidget::removeHotstpotAt(int index)
{
    if (d->isSelectedHotspotIndex(index))
        d->setSelectedHotspotIndex(std::nullopt);

    if (d->isHoveredHotspotIndex(index))
        d->setHoveredHotspotIndex(std::nullopt);

    d->hotspots.erase(std::next(d->hotspots.begin(), index));
    update();
    emit hotspotsDataChanged();
}

std::optional<int> CameraHotspotsEditorWidget::selectedHotspotIndex() const
{
    return d->selectedHotspotIndex;
}

void CameraHotspotsEditorWidget::setSelectedHotspotIndex(std::optional<int> index)
{
    if (d->selectedHotspotIndex == index)
        return;

    d->setSelectedHotspotIndex(index);
}

void CameraHotspotsEditorWidget::paintEvent(QPaintEvent* event)
{
    using namespace nx::vms::client::core;
    using namespace camera_hotspots;

    base_type::paintEvent(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    painter.fillRect(d->thumbnailRect(), colorTheme()->color("dark4"));
    if (!isEnabled())
        painter.setOpacity(nx::style::Hints::kDisabledItemOpacity);

    if (d->cameraThumbnail->status() == LiveCameraThumbnail::Status::loading)
        return;

    if (d->cameraThumbnail->status() == LiveCameraThumbnail::Status::ready)
    {
        const auto thumbnailImageSize =
            d->thumbnailRect().size() * painter.device()->devicePixelRatio();

        const auto thumbnailImage = d->cameraThumbnail->image()
            .scaled(thumbnailImageSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

        painter.drawImage(d->thumbnailRect(), thumbnailImage);
    }

    const auto systemContext = appContext()->currentSystemContext();
    if (!systemContext->accessController()->hasPermissions(
        d->cameraThumbnail->resource(), Qn::ViewLivePermission))
    {
        painter.setFont(noAccessPlaceholderFont());
        QnScopedPainterPenRollback guard(&painter, core::colorTheme()->color("red_core"));
        painter.drawText(rect(), tr("NO ACCESS"), {Qt::AlignCenter});
    }
    else if (d->cameraThumbnail->status() == LiveCameraThumbnail::Status::unavailable)
    {
        painter.setFont(noDataPlaceholderFont());
        painter.drawText(rect(), tr("NO DATA"), {Qt::AlignCenter});
    }

    for (int i = 0; i < d->hotspots.size(); ++i)
    {
        const auto hotspot = d->hotspots.at(i);

        CameraHotspotDisplayOption option;

        const auto displayedHotspotIndex = QString::number(i + 1);
        option.decoration = displayedHotspotIndex;

        if (d->isSelectedHotspotIndex(i))
            option.state = CameraHotspotDisplayOption::State::selected;
        else if (d->isHoveredHotspotIndex(i))
            option.state = CameraHotspotDisplayOption::State::hovered;

        if (!hotspot.cameraId.isNull())
        {
            option.cameraState = CameraHotspotDisplayOption::CameraState::valid;
            const auto resourcePool = systemContext->resourcePool();
            const auto camera =
                resourcePool->getResourceById<QnVirtualCameraResource>(hotspot.cameraId);

            if (!camera)
                option.cameraState = CameraHotspotDisplayOption::CameraState::invalid;
        }

        if (!isEnabled())
            option.state = CameraHotspotDisplayOption::State::disabled;

        paintHotspot(&painter, hotspot, hotspotOrigin(hotspot, d->thumbnailRect()), option);
    }
}

void CameraHotspotsEditorWidget::mouseMoveEvent(QMouseEvent* event)
{
    base_type::mouseMoveEvent(event);

    if (d->dragAction)
    {
        auto& draggedHotspot = d->hotspots[d->selectedHotspotIndex.value()];
        switch (d->dragAction.value())
        {
            case Private::DragAction::moveOrigin:
                setCursor(Qt::SizeAllCursor);
                camera_hotspots::setHotspotPositionFromPointInRect(
                    d->thumbnailRect(),
                    d->resolveHotspotsCollision(event->pos(), d->selectedHotspotIndex.value()),
                    draggedHotspot);
                break;

            case Private::DragAction::setDirection:
                setCursor(Qt::CrossCursor);
                draggedHotspot.direction = Geometry::normalized(event->pos()
                    - camera_hotspots::hotspotOrigin(draggedHotspot.pos, d->thumbnailRect()));
                break;
        }

        d->hasModificationsByDrag = true;
        update();
    }
    else
    {
        d->setHoveredHotspotIndex(d->hotspotIndexAtPos(event->pos()));
    }
}

void CameraHotspotsEditorWidget::mousePressEvent(QMouseEvent* event)
{
    base_type::mousePressEvent(event);

    if (event->buttons() != Qt::LeftButton)
        return;

    d->setHoveredHotspotIndex(d->hotspotIndexAtPos(event->pos()));

    if (d->hoveredHotspotIndex)
    {
        d->setSelectedHotspotIndex(d->hoveredHotspotIndex);
        d->dragAction = Private::DragAction::moveOrigin;

        const auto hotspot = d->hotspots.at(d->hoveredHotspotIndex.value());
        if (hotspot.hasDirection())
        {
            const auto originPoint = camera_hotspots::hotspotOrigin(hotspot, d->thumbnailRect());
            const auto pointerTipPoint
                = camera_hotspots::hotspotPointerTip(hotspot, d->thumbnailRect());

            if (Geometry::length(originPoint - event->pos())
                > Geometry::length(pointerTipPoint - event->pos()))
            {
                d->dragAction = Private::DragAction::setDirection;
            }
        }
    }
}

void CameraHotspotsEditorWidget::mouseReleaseEvent(QMouseEvent* event)
{
    base_type::mouseReleaseEvent(event);

    if (!event->buttons().testFlag(Qt::LeftButton))
    {
        d->dragAction.reset();
        setCursor(Qt::ArrowCursor);
    }

    if (d->hasModificationsByDrag)
    {
        d->hasModificationsByDrag = false;
        emit hotspotsDataChanged();
    }
}

void CameraHotspotsEditorWidget::wheelEvent(QWheelEvent* event)
{
    base_type::wheelEvent(event);

    if (event->angleDelta().y() == 0)
        return;

    if (d->dragAction || !d->hoveredHotspotIndex || !d->selectedHotspotIndex)
        return;

    if (d->hoveredHotspotIndex != d->selectedHotspotIndex)
        return;

    auto hotspotData = hotspotAt(d->selectedHotspotIndex.value());

    QTransform rotationMatrix;
    rotationMatrix.rotate(event->angleDelta().y() / 8.0);
    hotspotData.direction = rotationMatrix.map(hotspotData.direction);

    setHotspotAt(hotspotData, d->hoveredHotspotIndex.value());
}

} // namespace nx::vms::client::desktop
