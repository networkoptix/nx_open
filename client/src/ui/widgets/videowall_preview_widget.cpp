#include "videowall_preview_widget.h"

#include <client/client_settings.h>

#include <core/resource/videowall_resource.h>

#include <utils/common/string.h>
#include <utils/common/scoped_painter_rollback.h>

class ModelScreenPart {
public:
    ModelScreenPart(const QRect &rect):
        geometry(rect),
        free(true){}

    QRect geometry;
    bool free;
};


class QnVideowallModel {
public:
    QnVideoWallResourcePtr videowall;

    QList<ModelScreenPart> screenParts;
};

QnVideowallPreviewWidget::QnVideowallPreviewWidget(QWidget *parent /*= 0*/):
    QWidget(parent),
    m_model(new QnVideowallModel())
{
     QDesktopWidget* desktop = qApp->desktop();
     for (int i = 0; i < desktop->screenCount(); ++i) {
         QRect screen = desktop->screenGeometry(i);
         int width = screen.width() / QnScreenSnap::snapsPerScreen();
         int height = screen.height() / QnScreenSnap::snapsPerScreen();

         for (int j = 0; j < QnScreenSnap::snapsPerScreen(); ++j) {
             for (int k = 0; k < QnScreenSnap::snapsPerScreen(); ++k) {
                 QRect partRect(screen.x() + width * j, screen.y() + height * k, width, height);
                 m_model->screenParts << partRect;
             } 
         }

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

    targetRect = expanded(aspectRatio(unitedGeometry), targetRect, Qt::KeepAspectRatio).toRect();

    painter->fillRect(targetRect, palette().window());

    if (unitedGeometry.isNull())    //TODO: #GDM #VW replace by model.Valid
        return;

    // transform to paint in the desktop coordinate system
    QTransform scaleTransform(painter->transform());
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
    foreach (const ModelScreenPart &part, m_model->screenParts) {
        if (!part.free)
            continue;

        painter->fillRect(part.geometry, debugColors[idx]);
        idx = (idx + 1) % debugColors.size();
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

