#include "motion_regions_item.h"
#include "private/motion_regions_item_p.h"

#include <QtQml/QtQml>
#include <QtQuick/QSGGeometryNode>
#include <QtQuick/QSGTextureMaterial>

namespace nx::vms::client::desktop {

MotionRegionsItem::MotionRegionsItem(QQuickItem* parent):
    base_type(parent),
    d(new Private(this))
{
    setFlag(QQuickItem::ItemHasContents);
}

MotionRegionsItem::~MotionRegionsItem()
{
}

core::CameraMotionHelper* MotionRegionsItem::motionHelper() const
{
    return d->motionHelper();
}

void MotionRegionsItem::setMotionHelper(core::CameraMotionHelper* value)
{
    d->setMotionHelper(value);
}

int MotionRegionsItem::channel() const
{
    return d->channel();
}

void MotionRegionsItem::setChannel(int value)
{
    d->setChannel(value);
}

QVector<QColor> MotionRegionsItem::sensitivityColors() const
{
    return d->sensitivityColors();
}

void MotionRegionsItem::setSensitivityColors(const QVector<QColor>& value)
{
    d->setSensitivityColors(value);
}

QColor MotionRegionsItem::borderColor() const
{
    return d->borderColor();
}

void MotionRegionsItem::setBorderColor(const QColor& value)
{
    d->setBorderColor(value);
}

QColor MotionRegionsItem::labelsColor() const
{
    return d->labelsColor();
}

void MotionRegionsItem::setLabelsColor(const QColor& value)
{
    d->setLabelsColor(value);
}

qreal MotionRegionsItem::fillOpacity() const
{
    return d->fillOpacity();
}

void MotionRegionsItem::setFillOpacity(qreal value)
{
    d->setFillOpacity(value);
}

void MotionRegionsItem::registerQmlType()
{
    qmlRegisterType<MotionRegionsItem>("nx.client.desktop", 1, 0, "MotionRegions");
}

QSGNode* MotionRegionsItem::updatePaintNode(
    QSGNode* node, UpdatePaintNodeData* /*updatePaintNodeData*/)
{
    return d->updatePaintNode(node);
}

} // namespace nx::vms::client::desktop
