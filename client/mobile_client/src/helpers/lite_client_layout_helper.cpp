#include "lite_client_layout_helper.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_properties.h>
#include <core/resource_management/resources_changes_manager.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/camera_resource.h>

namespace {

const QString kDisplayCellProperty(lit("liteClientDisplayCell"));
const QPoint kInvalidPoint(-1, -1);

QString pointToString(const QPoint& point)
{
    return lit("%1,%2").arg(point.x()).arg(point.y());
}

QPoint pointFromString(const QString& str)
{
    const auto components = str.split(L',');
    if (components.size() != 2)
        return kInvalidPoint;

    return QPoint(components.first().toInt(), components.last().toInt());
}

QnLayoutItemData createItem(
    const QnUuid& resourceId,
    const QString& cameraUniqueId,
    const QRectF& combinedGeomtry)
{
    QnLayoutItemData result;
    result.uuid = QnUuid::createUuid();
    result.resource.id = resourceId;
    result.resource.uniqueId = cameraUniqueId;
    result.combinedGeometry = combinedGeomtry;
    return result;
};

} // namespace

class QnLiteClientLayoutHelperPrivate: public QObject
{
    QnLiteClientLayoutHelper* q_ptr;
    Q_DECLARE_PUBLIC(QnLiteClientLayoutHelper)

public:
    QnLiteClientLayoutHelperPrivate(QnLiteClientLayoutHelper* parent);

    void at_layoutPropertyChanged(const QnResourcePtr& resource, const QString& key);
    void at_layoutItemChanged(const QnResourcePtr& resource, const QnLayoutItemData& item);
    void at_layoutItemRemoved(const QnResourcePtr& resource, const QnLayoutItemData& item);

public:
    QnLayoutResourcePtr layout;
};

QnLiteClientLayoutHelper::QnLiteClientLayoutHelper(QObject* parent):
    base_type(parent),
    d_ptr(new QnLiteClientLayoutHelperPrivate(this))
{
}

QnLiteClientLayoutHelper::~QnLiteClientLayoutHelper()
{
}

QString QnLiteClientLayoutHelper::layoutId() const
{
    Q_D(const QnLiteClientLayoutHelper);
    return d->layout ? d->layout->getId().toString() : QString();
}

void QnLiteClientLayoutHelper::setLayoutId(const QString& layoutId)
{
    setLayout(resourcePool()->getResourceById<QnLayoutResource>(QnUuid::fromStringSafe(layoutId)));
}

QnLayoutResourcePtr QnLiteClientLayoutHelper::layout() const
{
    Q_D(const QnLiteClientLayoutHelper);
    return d->layout;
}

void QnLiteClientLayoutHelper::setLayout(const QnLayoutResourcePtr& layout)
{
    Q_D(QnLiteClientLayoutHelper);
    if (d->layout == layout)
        return;

    if (d->layout)
        disconnect(d->layout, nullptr, this, nullptr);

    d->layout = layout;
    emit layoutChanged();

    if (d->layout)
    {
        connect(d->layout, &QnLayoutResource::propertyChanged,
            d, &QnLiteClientLayoutHelperPrivate::at_layoutPropertyChanged);
        connect(d->layout, &QnLayoutResource::itemAdded,
            d, &QnLiteClientLayoutHelperPrivate::at_layoutItemChanged);
        connect(d->layout, &QnLayoutResource::itemChanged,
            d, &QnLiteClientLayoutHelperPrivate::at_layoutItemChanged);
        connect(d->layout, &QnLayoutResource::itemRemoved,
            d, &QnLiteClientLayoutHelperPrivate::at_layoutItemRemoved);

        emit displayCellChanged();
        emit singleCameraIdChanged();
    }
}

QPoint QnLiteClientLayoutHelper::displayCell() const
{
    Q_D(const QnLiteClientLayoutHelper);

    if (!d->layout)
        return kInvalidPoint;

    return pointFromString(d->layout->getProperty(kDisplayCellProperty));
}

void QnLiteClientLayoutHelper::setDisplayCell(const QPoint& cell)
{
    Q_D(QnLiteClientLayoutHelper);

    if (!d->layout)
        return;

    d->layout->setProperty(kDisplayCellProperty, pointToString(cell));

    propertyDictionary()->saveParams(d->layout->getId());
}

QnLiteClientLayoutHelper::DisplayMode QnLiteClientLayoutHelper::displayMode() const
{
    return displayCell() == kInvalidPoint
        ? DisplayMode::MultipleCameras : DisplayMode::SingleCamera;
}

QString QnLiteClientLayoutHelper::singleCameraId() const
{
    Q_D(const QnLiteClientLayoutHelper);

    if (!d->layout)
        return QString();

    const auto cell = displayCell();
    if (cell == kInvalidPoint)
        return QString();

    return cameraIdOnCell(cell.x(), cell.y());
}

void QnLiteClientLayoutHelper::setSingleCameraId(const QString& cameraId)
{
    Q_D(QnLiteClientLayoutHelper);

    if (!d->layout)
        return;

    const auto cell = displayCell();
    if (cell == kInvalidPoint)
        return;

    setCameraIdOnCell(cell.x(), cell.y(), cameraId);
}

QString QnLiteClientLayoutHelper::cameraIdOnCell(int x, int y) const
{
    Q_D(const QnLiteClientLayoutHelper);

    if (!d->layout)
        return QString();

    const auto items = d->layout->getItems();
    const auto it = std::find_if(items.begin(), items.end(),
        [x, y](const QnLayoutItemData& item)
        {
            return item.combinedGeometry.x() == x && item.combinedGeometry.y() == y;
        });

    if (it == items.end())
        return QString();

    return it->resource.id.toString();
}

void QnLiteClientLayoutHelper::setCameraIdOnCell(int x, int y, const QString& cameraId)
{
    Q_D(QnLiteClientLayoutHelper);

    if (!d->layout)
        return;

    const auto id = QnUuid::fromStringSafe(cameraId);
    const auto camera = resourcePool()->getResourceById<QnVirtualCameraResource>(id);

    auto items = d->layout->getItems();
    auto it = std::find_if(items.begin(), items.end(),
        [x, y](const QnLayoutItemData& item)
        {
            return item.combinedGeometry.x() == x && item.combinedGeometry.y() == y;
        });

    if (it == items.end())
    {
        if (id.isNull() || camera.isNull())
            return;

        d->layout->addItem(createItem(id, camera->getUniqueId(), QRectF(x, y, 1, 1)));
    }
    else
    {
        if (id.isNull() || camera.isNull())
        {
            d->layout->removeItem(*it);
        }
        else
        {
            if (it->resource.id == id)
                return;

            const auto current = *it;
            d->layout->removeItem(current);
            const auto newItem = createItem(id, camera->getUniqueId(), current.combinedGeometry);
            d->layout->addItem(newItem);
        }
    }

    qnResourcesChangesManager->saveLayout(d->layout, [](const QnLayoutResourcePtr&){});
}

QnLayoutResourcePtr QnLiteClientLayoutHelper::createLayoutForServer(const QnUuid& serverId) const
{
    const auto server = resourcePool()->getResourceById<QnMediaServerResource>(serverId);
    if (!server)
        return QnLayoutResourcePtr();

    auto layout = QnLayoutResourcePtr(new QnLayoutResource());
    layout->setId(QnUuid::createUuid());
    layout->setParentId(serverId);
    layout->setName(server->getName());

    resourcePool()->addResource(layout);
    resourcePool()->markLayoutLiteClient(layout);
    qnResourcesChangesManager->saveLayout(layout, [](const QnLayoutResourcePtr&){});

    return layout;
}

QnLayoutResourcePtr QnLiteClientLayoutHelper::findLayoutForServer(const QnUuid& serverId) const
{
    if (serverId.isNull())
        return QnLayoutResourcePtr();

    auto resources = resourcePool()->getResourcesByParentId(serverId)
        .filtered<QnLayoutResource>();

    if (resources.isEmpty())
        return QnLayoutResourcePtr();

    std::sort(resources.begin(), resources.end(),
        [](const QnLayoutResourcePtr& left, const QnLayoutResourcePtr& right)
        {
            return left->getId() < right->getId();
        });

    return *resources.begin();
}

QnLiteClientLayoutHelperPrivate::QnLiteClientLayoutHelperPrivate(QnLiteClientLayoutHelper* parent):
    QObject(parent),
    q_ptr(parent)
{
}

void QnLiteClientLayoutHelperPrivate::at_layoutPropertyChanged(
    const QnResourcePtr& resource, const QString& key)
{
    NX_ASSERT(resource == layout);
    if (resource != layout)
        return;

    Q_Q(QnLiteClientLayoutHelper);

    if (key == kDisplayCellProperty)
    {
        emit q->displayCellChanged();
        emit q->singleCameraIdChanged();
    }
}

void QnLiteClientLayoutHelperPrivate::at_layoutItemChanged(
    const QnResourcePtr& resource, const QnLayoutItemData& item)
{
    NX_ASSERT(resource == layout);
    if (resource != layout)
        return;

    Q_Q(QnLiteClientLayoutHelper);

    const auto x = static_cast<int>(item.combinedGeometry.x());
    const auto y = static_cast<int>(item.combinedGeometry.y());
    const auto id = item.resource.id.toString();
    emit q->cameraIdChanged(x, y, id);

    if (QPoint(x, y) == q->displayCell())
        emit q->singleCameraIdChanged();
}

void QnLiteClientLayoutHelperPrivate::at_layoutItemRemoved(
    const QnResourcePtr& resource, const QnLayoutItemData& item)
{
    NX_ASSERT(resource == layout);
    if (resource != layout)
        return;

    Q_Q(QnLiteClientLayoutHelper);

    const auto x = static_cast<int>(item.combinedGeometry.x());
    const auto y = static_cast<int>(item.combinedGeometry.y());
    emit q->cameraIdChanged(x, y, QString());

    if (QPoint(x, y) == q->displayCell())
        emit q->singleCameraIdChanged();
}
