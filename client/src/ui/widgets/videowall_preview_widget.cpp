#include "videowall_preview_widget.h"

#include <QtGui/QIcon>

#include <client/client_settings.h>

#include <core/resource/videowall_resource.h>

#include <ui/style/skin.h>

#include <utils/common/string.h>
#include <utils/common/scoped_painter_rollback.h>

namespace {
    class ModelScreenPart {
    public:
        ModelScreenPart(const QRect &rect):
            geometry(rect),
            free(true){}

        QRect geometry;
        bool free;
    };

    class ModelScreen {
    public:
        ModelScreen(const QRect &rect):
            geometry(rect)
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


        QRect geometry;
        QList<ModelScreenPart> parts;
    };

    const qreal frameMargin = 0.04;
    const qreal innerMargin = frameMargin * 2;
}

class QnVideowallModel {
public:
    QnVideoWallResourcePtr videowall;

    QList<ModelScreen> screens;
};

QnVideowallPreviewWidget::QnVideowallPreviewWidget(QWidget *parent /*= 0*/):
    QWidget(parent),
    m_model(new QnVideowallModel())
{
     QDesktopWidget* desktop = qApp->desktop();
     for (int i = 0; i < desktop->screenCount(); ++i) {
         m_model->screens << desktop->screenGeometry(i);
     }

}

QnVideowallPreviewWidget::~QnVideowallPreviewWidget() { }

void QnVideowallPreviewWidget::paintEvent(QPaintEvent *event) {
    QDesktopWidget* desktop = qApp->desktop();
    
    QList<QRect> screens;
    QRect unitedGeometry;
    screens.reserve( desktop->screenCount());
    for (int i = 0; i < desktop->screenCount(); i++) {
        screens << desktop->screenGeometry(i);
        unitedGeometry = unitedGeometry.united(desktop->screenGeometry(i));
    }

    QScopedPointer<QPainter> painter(new QPainter(this));

    QRect targetRect = event->rect();
    if (targetRect.isNull())
        return;

    painter->fillRect(targetRect, palette().window());

    targetRect = expanded(aspectRatio(unitedGeometry), targetRect, Qt::KeepAspectRatio).toRect();

    if (unitedGeometry.isNull())    //TODO: #GDM #VW replace by model.Valid
        return;

    // transform to paint in the desktop coordinate system
    QTransform scaleTransform(painter->transform());
    scaleTransform.translate(targetRect.left(), targetRect.top());
    scaleTransform.scale(
        static_cast<qreal> (targetRect.width()) /  unitedGeometry.width(),
        static_cast<qreal> (targetRect.height()) /  unitedGeometry.height());
    scaleTransform.translate(-unitedGeometry.left(), -unitedGeometry.top());

    QnScopedPainterTransformRollback scaleRollback(painter.data(), scaleTransform);

    //TODO: draw screen frames

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

    QList<QColor> debugColors;
    debugColors << Qt::red << Qt::blue << Qt::green << Qt::yellow;

    int idx = 0;
    foreach (const ModelScreen &screen, m_model->screens) {
        paintScreenFrame(painter.data(), screen.geometry);
        if (screen.free()) {
            paintPlaceholder(painter.data(), screen.geometry);
        } else {
            foreach (const ModelScreenPart &part, screen.parts) {
                if (part.free)
                    paintPlaceholder(painter.data(), part.geometry);
            }
        }
    }

    //TODO: draw placeholders with '+' sign

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


void QnVideowallPreviewWidget::paintScreenFrame(QPainter *painter, const QRect &geometry)
{
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


void QnVideowallPreviewWidget::paintPlaceholder(QPainter* painter, const QRect &geometry) {
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

    QRect iconRect(targetRect.center().toPoint(), QSize(200, 200));
    painter->drawPixmap(iconRect, qnSkin->pixmap("item/ptz_zoom_in.png"));
}

