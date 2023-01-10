// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_hotspots_editor_widget.h"

#include <QtGui/QPainter>
#include <QtGui/QPainterPath>
#include <QtGui/QWheelEvent>
#include <QtWidgets/QLayout>
#include <QtWidgets/QMenu>

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/core/utils/geometry.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/camera_hotspots/camera_hotspots_display_utils.h>
#include <nx/vms/client/desktop/common/utils/widget_anchor.h>
#include <nx/vms/client/desktop/common/widgets/busy_indicator.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/thumbnails/live_camera_thumbnail.h>
#include <nx/vms/client/desktop/ui/common/color_theme.h>
#include <ui/common/palette.h>
#include <ui/workaround/hidpi_workarounds.h>
#include <utils/math/math.h>

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

} // namespace

namespace nx::vms::client::desktop {

using Geometry = nx::vms::client::core::Geometry;
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
    bool isDragMove = false;
    bool isDragChange = false;

    QRect thumbnailRect() const;

    bool isSelectedHotspotIndex(int index) const;
    void setSelectedHotspotIndex(std::optional<int> index);

    bool isHoveredHotspotIndex(int index) const;
    void setHoveredHotspotIndex(std::optional<int> index);

    std::optional<int> hotspotIndexAtPos(const QPoint& pos) const;

    void processContextMenuRequest(const QPoint& pos);
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

    isDragMove = false;

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

    // if loading return

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
                CameraHotspotData hotspot;
                camera_hotspots::setHotspotPositionFromPointInRect(thumbnailRect(), pos, hotspot);
                q->appendHotspot(hotspot);
            });
    }

    QnHiDpiWorkarounds::showMenu(menu, QCursor::pos());
}


//-------------------------------------------------------------------------------------------------
// CameraHotspotEditorWidget definition.

CameraHotspotsEditorWidget::CameraHotspotsEditorWidget(QWidget* parent):
    base_type(parent),
    d(new Private{this})
{
    setMouseTracking(true);
    setContextMenuPolicy(Qt::CustomContextMenu);

    setPaletteColor(this, QPalette::Window, colorTheme()->color("dark5"));
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
    d->hotspots.push_back(hotspotData);
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

    painter.fillRect(d->thumbnailRect(), colorTheme()->color("dark4"));

    if (d->cameraThumbnail->status() == LiveCameraThumbnail::Status::loading)
        return;

    if (d->cameraThumbnail->status() == LiveCameraThumbnail::Status::ready)
        painter.drawImage(d->thumbnailRect(), d->cameraThumbnail->image());

    painter.setFont(noDataPlaceholderFont());
    if (d->cameraThumbnail->status() == LiveCameraThumbnail::Status::unavailable)
        painter.drawText(rect(), tr("NO DATA"), {Qt::AlignCenter});

    for (int i = 0; i < d->hotspots.size(); ++i)
    {
        const auto hotspot = d->hotspots.at(i);

        CameraHotspotDisplayOption option;
        option.rect = d->thumbnailRect();

        if (d->isSelectedHotspotIndex(i))
            option.state = CameraHotspotDisplayOption::State::selected;
        else if (d->isHoveredHotspotIndex(i))
            option.state = CameraHotspotDisplayOption::State::hovered;

        if (!hotspot.cameraId.isNull())
        {
            option.cameraState = CameraHotspotDisplayOption::CameraState::valid;
            const auto resourcePool = appContext()->currentSystemContext()->resourcePool();
            if (!resourcePool->getResourceById<QnVirtualCameraResource>(hotspot.cameraId))
                option.cameraState = CameraHotspotDisplayOption::CameraState::invalid;
        }

        paintHotspot(&painter, hotspot, option);
    }
}

void CameraHotspotsEditorWidget::mouseMoveEvent(QMouseEvent* event)
{
    base_type::mouseMoveEvent(event);

    if (d->isDragMove)
    {
        setCursor(Qt::SizeAllCursor);
        auto& draggedHotspot = d->hotspots[d->selectedHotspotIndex.value()];
        camera_hotspots::setHotspotPositionFromPointInRect(
            d->thumbnailRect(), event->pos(), draggedHotspot);
        d->isDragChange = true;
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

    if (d->hoveredHotspotIndex)
    {
        d->setSelectedHotspotIndex(d->hoveredHotspotIndex);
        d->isDragMove = true;
    }
}

void CameraHotspotsEditorWidget::mouseReleaseEvent(QMouseEvent* event) //discard drag move
{
    base_type::mouseReleaseEvent(event);

    if (!event->buttons().testFlag(Qt::LeftButton))
    {
        d->isDragMove = false;
        setCursor(Qt::ArrowCursor);
    }
    if (d->isDragChange)
    {
        d->isDragChange = false;
        emit hotspotsDataChanged();
    }
}

void CameraHotspotsEditorWidget::wheelEvent(QWheelEvent* event)
{
    base_type::wheelEvent(event);

    // TODO: #vbreus Implement QWheelEvent::pixelDelta() processing, implement high frequency wheel
    // events accumulation.

    if (event->angleDelta().y() == 0)
        return;

    if (d->isDragMove || !d->hoveredHotspotIndex || !d->selectedHotspotIndex)
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
