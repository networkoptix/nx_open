#include "videowall_preview_widget.h"

#include <QtGui/QIcon>

#include <client/client_settings.h>

#include <core/resource/videowall_resource.h>

#include <ui/style/skin.h>

#include <utils/common/string.h>
#include <utils/common/scoped_painter_rollback.h>

namespace {

    struct BaseModelItem {
        BaseModelItem(const QRect &rect):
            geometry(rect) {}

        QRect geometry;
        QPainterPath path;
    };


    struct ModelItem: BaseModelItem {
    };

    struct ModelScreenPart: BaseModelItem {
        ModelScreenPart(const QRect &rect):
            BaseModelItem(rect),
            free(true){}

        bool free;
    };

    struct ModelScreen: BaseModelItem {
        ModelScreen(const QRect &rect):
            BaseModelItem(rect)
        {
            int width = rect.width() / QnScreenSnap::snapsPerScreen();
            int height = rect.height() / QnScreenSnap::snapsPerScreen();

            for (int j = 0; j < QnScreenSnap::snapsPerScreen(); ++j) {
                for (int k = 0; k < QnScreenSnap::snapsPerScreen(); ++k) {
                    QRect partRect(rect.x() + width * j, rect.y() + height * k, width, height);
                    parts << partRect;
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

    const qreal frameMargin = 0.04;
    const qreal innerMargin = frameMargin * 2;
}

// TODO: refactor to Q_D
class QnVideowallModel: private QnGeometry {
public:
    QnVideoWallResourcePtr videowall;
    QRect unitedGeometry;

    QList<ModelItem> items;
    QList<ModelScreen> screens;

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

private:
    QTransform m_transform;
    QTransform m_invertedTransform;
    QRect m_widgetRect;
};

QnVideowallPreviewWidget::QnVideowallPreviewWidget(QWidget *parent /*= 0*/):
    base_type(parent),
    m_model(new QnVideowallModel())
{
    QRect unitedGeometry;
    QDesktopWidget* desktop = qApp->desktop();
    for (int i = 0; i < desktop->screenCount(); ++i) {
        QRect screen = desktop->screenGeometry(i);
        m_model->screens << screen;
        unitedGeometry = unitedGeometry.united(screen);
    }
    m_model->unitedGeometry = unitedGeometry;

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

    QList<QnVideoWallItem> smallItems, bigItems;
    foreach (const QnVideoWallItem &item, m_model->videowall->items()->getItems()) {
        if (item.screenSnaps.screens().size() > 1)
            bigItems << item;
        else
            smallItems << item;
    }

    foreach (const QnVideoWallItem &item, bigItems) {
        //TODO: draw big items
    }

    foreach (const QnVideoWallItem &item, smallItems) {
        //TODO: draw small items
    }

    int idx = 0;
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
}


void QnVideowallPreviewWidget::updateModel() {
    update();
}

void QnVideowallPreviewWidget::loadFromResource(const QnVideoWallResourcePtr &videowall) {
    m_model->videowall = videowall;
    updateModel();
}

void QnVideowallPreviewWidget::submitToResource(const QnVideoWallResourcePtr &videowall) {

    QUuid pcUuid = qnSettings->pcUuid();

    auto newItem = [&]() {
        QnVideoWallItem result;

        result.name = generateUniqueString([&videowall] () {
            QStringList used;
            foreach (const QnVideoWallItem &item, videowall->items()->getItems())
                used << item.name;
            return used;
        }(), tr("Screen"), tr("Screen %1") );
        result.pcUuid = pcUuid;
        result.uuid = QUuid::createUuid();
        return result;
    };

    QnVideoWallItem item = newItem();
    videowall->items()->addItem(item);
}


void QnVideowallPreviewWidget::paintScreenFrame(QPainter *painter, BaseModelItem &item)
{
    QRect geometry = item.geometry;

    qreal margin = frameMargin * qMin(geometry.width(), geometry.height());
    QRectF targetRect = eroded(geometry, margin);

    QPainterPath path;
    path.addRect(targetRect);
    item.path = path;

    QPen borderPen(QColor(130, 130, 130, 200));
    borderPen.setWidth(10);

    QnScopedPainterPenRollback penRollback(painter, borderPen);
    QnScopedPainterBrushRollback brushRollback(painter, QColor(130, 130, 130, 128));

    QnScopedPainterAntialiasingRollback antialiasingRollback(painter, false);

    painter->drawPath(path);
}


void QnVideowallPreviewWidget::paintPlaceholder(QPainter* painter, BaseModelItem &item) {
    QRect geometry = item.geometry;

    qreal margin = innerMargin * qMin(geometry.width(), geometry.height());

    QRectF targetRect = eroded(geometry, margin);

    QPainterPath path;
    path.addRoundRect(targetRect, 25);
    item.path = path;

    QPen borderPen(QColor(64, 130, 180, 200));
    borderPen.setWidth(25);
    
    QnScopedPainterPenRollback penRollback(painter, borderPen);
    QnScopedPainterBrushRollback brushRollback(painter, QColor(64, 130, 180, 128));
    QnScopedPainterAntialiasingRollback antialiasingRollback(painter, true);

    painter->drawPath(path);

    QRect iconRect(0, 0, 200, 200);
    iconRect.moveCenter(targetRect.center().toPoint());
    painter->drawPixmap(iconRect, qnSkin->pixmap("item/ptz_zoom_in.png"));
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

        QTransform transform(m_model->getInvertedTransform(m_model->targetRect(this->rect())));
        qDebug() << event->pos() << transform.map(event->pos());
        //addItem
    }
    //d->skipNextReleaseEvent = false;
}

void QnVideowallPreviewWidget::mouseMoveEvent(QMouseEvent *event) {
    QTransform transform(m_model->getInvertedTransform(m_model->targetRect(this->rect())));
    qDebug() << transform.map(event->pos());


}

