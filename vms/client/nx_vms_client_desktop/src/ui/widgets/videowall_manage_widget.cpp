// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "videowall_manage_widget.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtGui/QMouseEvent>

#include <core/resource/videowall_resource.h>
#include <nx/utils/string.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/settings/local_settings.h>
#include <ui/animation/animation_timer.h>
#include <ui/processors/drag_processor.h>
#include <ui/widgets/videowall_manage_widget_p.h>
#include <utils/common/scoped_painter_rollback.h>

using namespace nx::vms::client::desktop;

namespace {
    const QSize minimumWidgetSizeHint(400, 300);
} // namespace

QnVideowallManageWidget::QnVideowallManageWidget(QWidget* parent /* = 0*/):
    base_type(parent),
    d_ptr(new QnVideowallManageWidgetPrivate(this)),
    m_dragProcessor(new DragProcessor(this)),
    m_skipReleaseEvent(false),
    m_pressedButtons(Qt::NoButton)
{
    installEventFilter(this);
    setMouseTracking(true);

    m_dragProcessor->setHandler(this);

    auto animationTimer = new QtBasedAnimationTimer(this);
    connect(animationTimer, &QtBasedAnimationTimer::tick, this, &QnVideowallManageWidget::tick);
    animationTimer->start();

    connect(d_ptr.data(), &QnVideowallManageWidgetPrivate::itemsChanged,
        this, &QnVideowallManageWidget::itemsChanged);
}

QnVideowallManageWidget::~QnVideowallManageWidget()
{
}

void QnVideowallManageWidget::paintEvent(QPaintEvent* /*event*/)
{
    Q_D(QnVideowallManageWidget);

    QScopedPointer<QPainter> painter(new QPainter(this));

    QRect paintRect = this->rect();
    if (paintRect.isNull())
        return;

    painter->fillRect(paintRect, palette().window());
    d->paint(painter.data(), paintRect);
}

void QnVideowallManageWidget::loadFromResource(const QnVideoWallResourcePtr& videowall)
{
    videowall::Model model;
    const QnUuid pcUuid = appContext()->localSettings()->pcUuid();
    for (const QnVideoWallItem& item: videowall->items()->getItems())
    {
        if (item.pcUuid != pcUuid)
            continue;

        model.items.append(videowall::Item{item.uuid, item.name, item.screenSnaps});
    }

    Q_D(QnVideowallManageWidget);
    d->setModel(model);
    update();
}

void QnVideowallManageWidget::submitToResource(const QnVideoWallResourcePtr& videowall)
{
    Q_D(QnVideowallManageWidget);

    const auto model = d->model();

    QList<QnVideoWallPcData::PcScreen> localScreens;
    for (auto i = 0; i < model.screens.size(); i++)
    {
        QnVideoWallPcData::PcScreen screen;
        screen.index = i;
        screen.desktopGeometry = model.screens[i];
        localScreens << screen;
    }

    QnUuid pcUuid = appContext()->localSettings()->pcUuid();
    QnVideoWallPcData pcData;
    pcData.uuid = pcUuid;
    pcData.screens = localScreens;
    videowall->pcs()->addOrUpdateItem(pcData);

    /* Form list of items currently attached to this PC. */
    QSet<QnUuid> existingIds;
    for (const QnVideoWallItem& item: videowall->items()->getItems())
    {
        if (item.pcUuid == pcUuid)
            existingIds << item.uuid;
    }

    /* Remove from the list items that will be updated. */
    for (const auto& modelItem: model.items)
    {
        existingIds.remove(modelItem.id);

        QnVideoWallItem item;
        if (modelItem.isAdded || !videowall->items()->hasItem(modelItem.id))
        {
            item.name = nx::utils::generateUniqueString(
                [&videowall]()
                {
                    QStringList used;
                    for (const QnVideoWallItem& item : videowall->items()->getItems())
                        used << item.name;
                    return used;
                }(),
                tr("Screen"),
                tr("Screen %1"));
            item.pcUuid = pcUuid;
            item.uuid = modelItem.id;
        }
        else
        {
            item = videowall->items()->getItem(modelItem.id);
        }
        item.screenSnaps = modelItem.snaps;
        videowall->items()->addOrUpdateItem(item);
    }

    /* Delete other items. */
    for (const QnUuid& toDelete: existingIds)
        videowall->items()->removeItem(toDelete);
}

bool QnVideowallManageWidget::eventFilter(QObject* target, QEvent* event)
{
    if (event->type() == QEvent::LeaveWhatsThisMode)
        m_skipReleaseEvent = true;
    else if (event->type() == QEvent::Enter)
    {
        Q_D(QnVideowallManageWidget);
        d->mouseEnter();
    }
    else if (event->type() == QEvent::Leave)
    {
        Q_D(QnVideowallManageWidget);
        d->mouseLeave();
    }
    m_dragProcessor->widgetEvent(this, event);
    return base_type::eventFilter(target, event);
}

void QnVideowallManageWidget::mousePressEvent(QMouseEvent* event)
{
    base_type::mousePressEvent(event);
    m_pressedButtons = event->buttons();
}

void QnVideowallManageWidget::mouseMoveEvent(QMouseEvent* event)
{
    base_type::mouseMoveEvent(event);

    if (m_dragProcessor->isRunning())
        return;

    Q_D(QnVideowallManageWidget);

    QTransform transform(d->getInvertedTransform(d->targetRect(this->rect())));
    d->mouseMoveAt(transform.map(event->pos()));
}

void QnVideowallManageWidget::mouseReleaseEvent(QMouseEvent* event)
{
    base_type::mouseReleaseEvent(event);

    if (m_skipReleaseEvent)
    {
        m_skipReleaseEvent = false;
        return;
    }

    Q_D(QnVideowallManageWidget);
    QTransform transform(d->getInvertedTransform(d->targetRect(this->rect())));
    QPoint p = transform.map(event->pos());
    d->mouseClickAt(p, m_pressedButtons);
}


void QnVideowallManageWidget::startDrag(DragInfo* info)
{
    m_skipReleaseEvent = true;

    Q_D(QnVideowallManageWidget);
    QTransform transform(d->getInvertedTransform(d->targetRect(this->rect())));

    QPoint p = transform.map(mapFromGlobal(info->mouseScreenPos()));
    d->dragStartAt(p);
}

void QnVideowallManageWidget::dragMove(DragInfo* info)
{
    Q_D(QnVideowallManageWidget);
    QTransform transform(d->getInvertedTransform(d->targetRect(this->rect())));
    QPoint p = transform.map(mapFromGlobal(info->mouseScreenPos()));
    QPoint prev = transform.map(mapFromGlobal(info->lastMouseScreenPos()));
    d->dragMoveAt(p, prev);
}

void QnVideowallManageWidget::finishDrag(DragInfo* info)
{
    Q_D(QnVideowallManageWidget);
    QTransform transform(d->getInvertedTransform(d->targetRect(this->rect())));
    QPoint p = transform.map(mapFromGlobal(info->mouseScreenPos()));
    d->dragEndAt(p);
}

void QnVideowallManageWidget::tick(int deltaTime)
{
    Q_D(QnVideowallManageWidget);
    d->tick(deltaTime);
    update();
}

QList<QRect> QnVideowallManageWidget::screenGeometries() const
{
    return m_screenGeometries;
}

void QnVideowallManageWidget::setScreenGeometries(const QList<QRect>& value)
{
    m_screenGeometries = value;
    d_ptr->initScreenGeometries();
}

nx::vms::client::desktop::videowall::Model QnVideowallManageWidget::model()
{
    Q_D(const QnVideowallManageWidget);
    return d->model();
}

QString QnVideowallManageWidget::jsonModel()
{
    // Write Screens.
    QJsonArray jsonScreens;
    for (auto const& screen: model().screens)
    {
        QJsonObject jsonScreen;
        jsonScreen["x"] = screen.x();
        jsonScreen["y"] = screen.y();
        jsonScreen["width"] = screen.width();
        jsonScreen["height"] = screen.height();
        jsonScreens.append(jsonScreen);
    }
    // Write Items.
    QJsonArray jsonItems;
    for (auto const& item: model().items)
    {
        QJsonObject jsonItem;
        jsonItem["uuid"] = item.id.toString();
        jsonItem["name"] = item.name;
        jsonItem["added"] = item.isAdded;
        // Write ScreenSnaps.
        QJsonObject jsonSnaps;
        QStringList snapNames = {"left", "right", "top", "bottom"};
        for (int i = 0; i < 4; i++)
        {
            QJsonObject jsonSnap;
            jsonSnap["screenIndex"] = item.snaps.values[i].screenIndex;
            jsonSnap["snapIndex"] = item.snaps.values[i].snapIndex;
            jsonSnaps[snapNames[i]] = jsonSnap;
        }
        jsonItem["snaps"] = jsonSnaps;
        jsonItems.append(jsonItem);
    }
    // Combine everything.
    QJsonObject jsonModel;
    jsonModel["screens"] = jsonScreens;
    jsonModel["items"] = jsonItems;

    const QJsonDocument jsonDoc(jsonModel);
    return jsonDoc.toJson();
}

int QnVideowallManageWidget::proposedItemsCount() const
{
    Q_D(const QnVideowallManageWidget);
    return d->proposedItemsCount();
}

QSize QnVideowallManageWidget::minimumSizeHint() const
{
    Q_D(const QnVideowallManageWidget);
    QRect source(QPoint(0, 0), minimumWidgetSizeHint);
    return d->targetRect(source).size();
}
