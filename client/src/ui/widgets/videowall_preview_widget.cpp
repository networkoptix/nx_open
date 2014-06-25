#include "videowall_preview_widget.h"

#include <QtGui/QIcon>

#include <client/client_settings.h>

#include <core/resource/videowall_resource.h>

#include <ui/style/skin.h>

#include <utils/common/string.h>
#include <utils/common/scoped_painter_rollback.h>


namespace {
    const qreal frameMargin = 0.04;
    const qreal innerMargin = frameMargin * 2;
}

enum class ItemType {
    Existing,
    ScreenPart,
    Screen,
    Added
};

struct BaseModelItem {
    BaseModelItem(const QRect &rect):
        geometry(rect) {}

    QUuid id;
    QRect geometry;
    ItemType itemType;
    QnScreenSnaps snaps;
};


struct ModelItem: BaseModelItem {
    ModelItem(): BaseModelItem(QRect()) {

    }

};

struct ModelScreenPart: BaseModelItem {
    ModelScreenPart(int screenIdx, int xIndex, int yIndex, const QRect &rect):
        BaseModelItem(rect),
        free(true)
    {
        id = QUuid::createUuid();
        itemType = ItemType::ScreenPart;
        for (auto snap = snaps.values.begin(); snap != snaps.values.end(); ++snap) {
            snap->screenIndex = screenIdx;
        }
        snaps.left().snapIndex = xIndex;
        snaps.top().snapIndex = yIndex;
        snaps.right().snapIndex = QnScreenSnap::snapsPerScreen() - xIndex - 1;
        snaps.bottom().snapIndex = QnScreenSnap::snapsPerScreen() - yIndex - 1;
    }

    bool free;
};

struct ModelScreen: BaseModelItem {
    ModelScreen(int idx, const QRect &rect):
        BaseModelItem(rect)
    {
        id = QUuid::createUuid();
        itemType = ItemType::Screen;
        for (auto snap = snaps.values.begin(); snap != snaps.values.end(); ++snap) {
            snap->screenIndex = idx;
            snap->snapIndex = 0;
        }

        int width = rect.width() / QnScreenSnap::snapsPerScreen();
        int height = rect.height() / QnScreenSnap::snapsPerScreen();

        for (int j = 0; j < QnScreenSnap::snapsPerScreen(); ++j) {
            for (int k = 0; k < QnScreenSnap::snapsPerScreen(); ++k) {
                QRect partRect(rect.x() + width * j, rect.y() + height * k, width, height);
                ModelScreenPart part(idx, j, k, partRect);
                parts << part;
            } 
        }
    }


    bool free() const {
        return std::all_of(parts.cbegin(), parts.cend(), [](const ModelScreenPart &part) {return part.free;});
    }

    void setFree(bool value) {
        std::transform(parts.begin(), parts.end(), parts.begin(), [value](ModelScreenPart part) {
            part.free = value;
            return part;
        });
    }

    QList<ModelScreenPart> parts;
};


// TODO: refactor to Q_D
class QnVideowallModel: private QnGeometry {
public:
    QnVideoWallResourcePtr videowall;
    QRect unitedGeometry;

    QList<ModelItem> items;
    QList<ModelScreen> screens;
    ModelItem addedItem;

    QnVideowallModel() {
        addedItem.itemType = ItemType::Added;

        QDesktopWidget* desktop = qApp->desktop();
        for (int i = 0; i < desktop->screenCount(); ++i) {
            ModelScreen screen(i, desktop->screenGeometry(i));
            screens << screen;

            unitedGeometry = unitedGeometry.united(screen.geometry);
        }
    }

    QTransform getTransform(const QRect &rect) {
        if (m_widgetRect == rect)
            return m_transform;

        m_transform.reset();
        m_transform.translate(rect.left(), rect.top());
        m_transform.scale(
            static_cast<qreal> (rect.width()) /  unitedGeometry.width(),
            static_cast<qreal> (rect.height()) /  unitedGeometry.height());
        m_transform.translate(-unitedGeometry.left(), -unitedGeometry.top());
        m_invertedTransform = m_transform.inverted();

        m_widgetRect = rect;
        return m_transform;
    }

    QTransform getInvertedTransform(const QRect &rect) {
        if (m_widgetRect != rect)
            getTransform(rect);
        return m_invertedTransform;
    }

    QRect targetRect(const QRect &rect) const {
        if (unitedGeometry.isNull())    //TODO: #GDM #VW replace by model.Valid
            return QRect();

        return expanded(aspectRatio(unitedGeometry), rect, Qt::KeepAspectRatio).toRect();
    }

    QUuid itemIdByPos(const QPoint &pos) {
        foreach (const ModelItem &item, items) {
            if (!item.geometry.contains(pos))
                continue;
            return item.id;
        }

        foreach (const ModelScreen &screen, screens) {
            if (!screen.geometry.contains(pos))
                continue;
            if (screen.free())
                return screen.id;
            foreach (const ModelScreenPart &part, screen.parts) {
                if (!part.geometry.contains(pos))
                    continue;
                return part.id;
            }
        }

        if (addedItem.geometry.contains(pos))
            return addedItem.id;

        return QUuid();
    }

    void addItemTo(const QUuid &id) {

        for (auto screen = screens.begin(); screen != screens.end(); ++screen) {
            if (screen->id == id) {
                addedItem.id = QUuid::createUuid();
                addedItem.geometry = screen->geometry;
                addedItem.snaps = screen->snaps;
                screen->setFree(false);
                return;
            }
               
            if (screen->free())
                continue;

            for (auto part = screen->parts.begin(); part != screen->parts.end(); ++part) {
                if (part->id != id)
                    continue;
                addedItem.id = QUuid::createUuid();
                addedItem.geometry = part->geometry;
                addedItem.snaps = part->snaps;
                part->free = false;
                return;
            }
        }
    }

    void use(const QnScreenSnaps &snaps) {
        QSet<int> screenIdxs = snaps.screens();
        // if the item takes some screens, it should take them fully
        if (screenIdxs.size() > 1) {
            for (int idx: screenIdxs) {
                if (idx >= screens.size())
                    continue;   // we can lose one screen after snaps have been set
                assert(screens[idx].free());
                screens[idx].setFree(false);
            }
        } else if (!screenIdxs.isEmpty()) { //otherwise looking at the screen parts
            int screenIdx = screenIdxs.toList().first();
            if (screenIdx >= screens.size())
                return;
            ModelScreen &screen = screens[screenIdx];

            for (int i = snaps.left().snapIndex; i < QnScreenSnap::snapsPerScreen() - snaps.right().snapIndex; ++i) {
                for (int j = snaps.top().snapIndex; j < QnScreenSnap::snapsPerScreen() - snaps.bottom().snapIndex; ++j) {
                    int idx = i* QnScreenSnap::snapsPerScreen() + j;
                    assert(idx < screen.parts.size());
                    assert(screen.parts[idx].free);
                    screen.parts[idx].free = false;
                }
            }
        }

    }


private:
    QTransform m_transform;
    QTransform m_invertedTransform;
    QRect m_widgetRect;
};

QnVideowallPreviewWidget::QnVideowallPreviewWidget(QWidget *parent /*= 0*/):
    base_type(parent),
    m_model(new QnVideowallModel())
{


    installEventFilter(this);
}

QnVideowallPreviewWidget::~QnVideowallPreviewWidget() { }

void QnVideowallPreviewWidget::paintEvent(QPaintEvent *event) {
    QScopedPointer<QPainter> painter(new QPainter(this));

    QRect eventRect = event->rect();
    if (eventRect.isNull())
        return;

    painter->fillRect(eventRect, palette().window());

    if (m_model->unitedGeometry.isNull())    //TODO: #GDM #VW replace by model.Valid
        return;

    QRect targetRect = m_model->targetRect(eventRect);

    // transform to paint in the desktop coordinate system
    QTransform transform(m_model->getTransform(targetRect));

    QnScopedPainterTransformRollback transformRollback(painter.data(), transform);

    if (!m_model->videowall)
        return;

    QList<BaseModelItem> smallItems, bigItems;
    foreach (const BaseModelItem &item, m_model->items) {
        if (item.snaps.screens().size() > 1)
            bigItems << item;
        else
            smallItems << item;
    }

    foreach (const BaseModelItem &item, bigItems) {
        paintExistingItem(painter.data(), item);
    }

    foreach (const BaseModelItem &item, smallItems) {
        paintExistingItem(painter.data(), item);
    }

    foreach (const ModelScreen &screen, m_model->screens) {
        paintScreenFrame(painter.data(), screen);
        if (screen.free()) {
            paintPlaceholder(painter.data(), screen);
        } else {
            foreach (const ModelScreenPart &part, screen.parts) {
                if (part.free)
                    paintPlaceholder(painter.data(), part);
            }
        }
    }

     if (!m_model->addedItem.id.isNull())
         paintAddedItem(painter.data());
}


void QnVideowallPreviewWidget::updateModel() {
    update();
}

void QnVideowallPreviewWidget::loadFromResource(const QnVideoWallResourcePtr &videowall) {

    QList<QRect> screens;
    QDesktopWidget* desktop = qApp->desktop();
    for (int i = 0; i < desktop->screenCount(); ++i)
        screens << desktop->screenGeometry(i);

    m_model->videowall = videowall;
    foreach (const QnVideoWallItem &item, videowall->items()->getItems()) {
        ModelItem modelItem;
        modelItem.id = item.uuid;
        modelItem.itemType = ItemType::Existing;
        modelItem.snaps = item.screenSnaps;
        modelItem.geometry = item.screenSnaps.geometry(screens);
        m_model->items << modelItem;
        m_model->use(modelItem.snaps);
    }

    updateModel();
}

void QnVideowallPreviewWidget::submitToResource(const QnVideoWallResourcePtr &videowall) {

    QUuid pcUuid = qnSettings->pcUuid();

    QList<QnVideoWallPcData::PcScreen> localScreens;
    for (int i = 0; i < m_model->screens.size(); i++) {
        QnVideoWallPcData::PcScreen screen;
        screen.index = i;
        screen.desktopGeometry = m_model->screens[i].geometry;
        localScreens << screen;
    }

    QnVideoWallPcData pcData;
    pcData.uuid = pcUuid;
    pcData.screens = localScreens;
    if (!videowall->pcs()->hasItem(pcUuid))
        videowall->pcs()->addItem(pcData);
    else
        videowall->pcs()->updateItem(pcUuid, pcData);

    if (m_model->addedItem.id.isNull())
        return;

    // encapsulating into lambda because there is a future possibility of adding several items at once
    auto newItem = [&videowall, &pcUuid, this]() {
        QnVideoWallItem result;

        result.name = generateUniqueString([&videowall] () {
            QStringList used;
            foreach (const QnVideoWallItem &item, videowall->items()->getItems())
                used << item.name;
            return used;
        }(), tr("Screen"), tr("Screen %1") );
        result.pcUuid = pcUuid;
        result.uuid = m_model->addedItem.id;
        result.screenSnaps = m_model->addedItem.snaps;
        return result;
    };

    QnVideoWallItem item = newItem();
    videowall->items()->addItem(item);



}


void QnVideowallPreviewWidget::paintScreenFrame(QPainter *painter, const BaseModelItem &item)
{
    QRect geometry = item.geometry;

    qreal margin = frameMargin * qMin(geometry.width(), geometry.height());
    QRectF targetRect = eroded(geometry, margin);

    QPainterPath path;
    path.addRect(targetRect);

    QPen borderPen(QColor(130, 130, 130, 200));
    borderPen.setWidth(10);

    QnScopedPainterPenRollback penRollback(painter, borderPen);
    QnScopedPainterBrushRollback brushRollback(painter, QColor(130, 130, 130, 128));

    QnScopedPainterAntialiasingRollback antialiasingRollback(painter, false);

    painter->drawPath(path);
}


void QnVideowallPreviewWidget::paintPlaceholder(QPainter* painter, const BaseModelItem &item) {
    QRect geometry = item.geometry;

    qreal margin = innerMargin * qMin(geometry.width(), geometry.height());

    QRectF targetRect = eroded(geometry, margin);

    QPainterPath path;
    path.addRoundRect(targetRect, 25);

    QPen borderPen(QColor(64, 130, 180, 200));
    borderPen.setWidth(25);
    
    QnScopedPainterPenRollback penRollback(painter, borderPen);
    QnScopedPainterBrushRollback brushRollback(painter, QColor(64, 130, 180, 128));
    QnScopedPainterAntialiasingRollback antialiasingRollback(painter, true);

    painter->drawPath(path);

    if (!m_model->addedItem.id.isNull())
        return;

    // no item added yet, paint "Add" button
    QRect iconRect(0, 0, 200, 200);
    iconRect.moveCenter(targetRect.center().toPoint());
    painter->drawPixmap(iconRect, qnSkin->pixmap("item/ptz_zoom_in.png"));
}


void QnVideowallPreviewWidget::paintExistingItem(QPainter* painter, const BaseModelItem &item) {
    QRect geometry = item.geometry;

    qreal margin = innerMargin * qMin(geometry.width(), geometry.height());

    QRectF targetRect = eroded(geometry, margin);

    QPainterPath path;
    path.addRoundRect(targetRect, 25);

    QPen borderPen(QColor(64, 64, 64, 200));
    borderPen.setWidth(25);

    QnScopedPainterPenRollback penRollback(painter, borderPen);
    QnScopedPainterBrushRollback brushRollback(painter, QColor(64, 64, 64, 128));
    QnScopedPainterAntialiasingRollback antialiasingRollback(painter, true);

    painter->drawPath(path);

}

void QnVideowallPreviewWidget::paintAddedItem(QPainter* painter) {
    BaseModelItem item = m_model->addedItem;
    QRect geometry = item.geometry;

    qreal margin = innerMargin * qMin(geometry.width(), geometry.height());

    QRectF targetRect = eroded(geometry, margin);

    QPainterPath path;
    path.addRoundRect(targetRect, 25);

    QPen borderPen(QColor(64, 180, 130, 200));
    borderPen.setWidth(25);

    QnScopedPainterPenRollback penRollback(painter, borderPen);
    QnScopedPainterBrushRollback brushRollback(painter, QColor(64, 180, 130, 128));
    QnScopedPainterAntialiasingRollback antialiasingRollback(painter, true);

    painter->drawPath(path);
}


bool QnVideowallPreviewWidget::eventFilter(QObject *target, QEvent *event) {
//     if (event->type() == QEvent::LeaveWhatsThisMode)
//         d->skipNextReleaseEvent = true;
    return base_type::eventFilter(target, event);
}

void QnVideowallPreviewWidget::mouseReleaseEvent(QMouseEvent *event)
{
    /* if (&& !d->skipNextReleaseEvent) */
    {
        if (!m_model->addedItem.id.isNull())
            return; //item already added

        QTransform transform(m_model->getInvertedTransform(m_model->targetRect(this->rect())));
        QPoint p = transform.map(event->pos());
        QUuid id = m_model->itemIdByPos(p);
        if (id.isNull())
            return;

        m_model->addItemTo(id);
        update();
    }
    //d->skipNextReleaseEvent = false;
}

void QnVideowallPreviewWidget::mouseMoveEvent(QMouseEvent *event) {
    QTransform transform(m_model->getInvertedTransform(m_model->targetRect(this->rect())));
    qDebug() << transform.map(event->pos());



}

