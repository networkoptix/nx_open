#include "videowall_preview_widget.h"

#include <client/client_settings.h>

#include <core/resource/videowall_resource.h>

#include <utils/common/string.h>

QnVideowallPreviewWidget::QnVideowallPreviewWidget(QWidget *parent /*= 0*/):
    QWidget(parent) {
    updateModel();
}

QnVideowallPreviewWidget::~QnVideowallPreviewWidget() { }

void QnVideowallPreviewWidget::paintEvent(QPaintEvent *event) {
    QScopedPointer<QPainter> painter(new QPainter(this));

    QRect targetRect = event->rect();
    painter->fillRect(targetRect, palette().window());


}
void QnVideowallPreviewWidget::updateModel() {
    update();
}

void QnVideowallPreviewWidget::loadFromResource(const QnVideoWallResourcePtr &videowall) {
    updateModel();
}

void QnVideowallPreviewWidget::submitToResource(const QnVideoWallResourcePtr &videowall) {
    QDesktopWidget* desktop = qApp->desktop();
    QList<QnVideoWallPcData::PcScreen> localScreens;
    QRect unitedGeometry;
    for (int i = 0; i < desktop->screenCount(); i++) {
        QnVideoWallPcData::PcScreen screen;
        screen.index = i;
        screen.desktopGeometry = desktop->screenGeometry(i);
        unitedGeometry = unitedGeometry.united(screen.desktopGeometry);
        localScreens << screen;
    }
    
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

