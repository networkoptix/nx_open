#include "lite_client_layout_helper.h"

#include <nx/utils/pending_operation.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_properties.h>
#include <core/resource_management/resources_changes_manager.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/camera_resource.h>

namespace {

const QString kDisplayCellProperty(lit("liteClientDisplayCell"));
const QPoint kInvalidPoint(-1, -1);
constexpr int kSaveLayoutTimeout = 500;

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
}

} // namespace

namespace nx {
namespace client {
namespace mobile {
namespace resource {

class LiteClientLayoutHelper::Private: public Connective<QObject>
{
    LiteClientLayoutHelper* q;

public:
    Private(LiteClientLayoutHelper* parent);

    void at_layoutAboutToBeChanged();
    void at_layoutChanged();
    void at_layoutPropertyChanged(const QnResourcePtr& resource, const QString& key);
    void at_layoutItemChanged(const QnResourcePtr& resource, const QnLayoutItemData& item);
    void at_layoutItemRemoved(const QnResourcePtr& resource, const QnLayoutItemData& item);

    void saveLayout();

public:
    utils::PendingOperation* saveOperation = nullptr;
};

LiteClientLayoutHelper::LiteClientLayoutHelper(QObject* parent):
    base_type(parent),
    d(new Private(this))
{
    connect(this, &LiteClientLayoutHelper::layoutAboutToBeChanged,
        d, &Private::at_layoutAboutToBeChanged);
    connect(this, &LiteClientLayoutHelper::layoutChanged,
        d, &Private::at_layoutChanged);
}

LiteClientLayoutHelper::~LiteClientLayoutHelper()
{
}

QPoint LiteClientLayoutHelper::displayCell() const
{
    const auto layout = this->layout();

    if (!layout)
        return kInvalidPoint;

    return pointFromString(layout->getProperty(kDisplayCellProperty));
}

void LiteClientLayoutHelper::setDisplayCell(const QPoint& cell)
{
    const auto layout = this->layout();

    if (!layout)
        return;

    layout->setProperty(kDisplayCellProperty, pointToString(cell));

    propertyDictionary()->saveParams(layout->getId());
}

LiteClientLayoutHelper::DisplayMode LiteClientLayoutHelper::displayMode() const
{
    return displayCell() == kInvalidPoint
        ? DisplayMode::MultipleCameras : DisplayMode::SingleCamera;
}

QString LiteClientLayoutHelper::singleCameraId() const
{
    const auto cell = displayCell();
    if (cell == kInvalidPoint)
        return QString();

    return cameraIdOnCell(cell.x(), cell.y());
}

void LiteClientLayoutHelper::setSingleCameraId(const QString& cameraId)
{
    const auto cell = displayCell();
    if (cell == kInvalidPoint)
        return;

    setCameraIdOnCell(cell.x(), cell.y(), cameraId);
}

QString LiteClientLayoutHelper::cameraIdOnCell(int x, int y) const
{
    const auto layout = this->layout();

    if (!layout)
        return QString();

    for (const auto& item: layout->getItems())
    {
        if (item.combinedGeometry.topLeft().toPoint() == QPoint(x, y))
            return item.resource.id.toString();
    }

    return QString();
}

void LiteClientLayoutHelper::setCameraIdOnCell(int x, int y, const QString& cameraId)
{
    const auto layout = this->layout();

    if (!layout)
        return;

    const auto id = QnUuid::fromStringSafe(cameraId);
    const auto camera = resourcePool()->getResourceById<QnVirtualCameraResource>(id);

    for (const auto& item: layout->getItems())
    {
        if (item.combinedGeometry.topLeft().toPoint() == QPoint(x, y))
            layout->removeItem(item);
    }

    if (id.isNull() || camera.isNull())
        return;

    layout->addItem(createItem(id, camera->getUniqueId(), QRectF(x, y, 1, 1)));

    d->saveOperation->requestOperation();
}

QnLayoutResourcePtr LiteClientLayoutHelper::createLayoutForServer(const QnUuid& serverId) const
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

QnLayoutResourcePtr LiteClientLayoutHelper::findLayoutForServer(const QnUuid& serverId) const
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

LiteClientLayoutHelper::Private::Private(LiteClientLayoutHelper* parent):
    q(parent),
    saveOperation(new utils::PendingOperation([this]{ saveLayout(); }, kSaveLayoutTimeout, this))
{
    saveOperation->setFlags(utils::PendingOperation::FireOnlyWhenIdle);
}

void LiteClientLayoutHelper::Private::at_layoutAboutToBeChanged()
{
    const auto layout = q->layout();
    if (!layout)
        return;

    layout->disconnect(this);
}

void LiteClientLayoutHelper::Private::at_layoutChanged()
{
    const auto layout = q->layout();

    if (!layout)
        return;

    connect(layout, &QnLayoutResource::propertyChanged,
        this, &Private::at_layoutPropertyChanged);
    connect(layout, &QnLayoutResource::itemAdded,
        this, &Private::at_layoutItemChanged);
    connect(layout, &QnLayoutResource::itemChanged,
        this, &Private::at_layoutItemChanged);
    connect(layout, &QnLayoutResource::itemRemoved,
        this, &Private::at_layoutItemRemoved);

    emit q->displayCellChanged();
    emit q->singleCameraIdChanged();
}

void LiteClientLayoutHelper::Private::at_layoutPropertyChanged(
    const QnResourcePtr& resource, const QString& key)
{
    if (resource != q->layout())
    {
        NX_ASSERT(resource == q->layout());
        return;
    }

    if (key == kDisplayCellProperty)
    {
        emit q->displayCellChanged();
        emit q->singleCameraIdChanged();
    }
}

void LiteClientLayoutHelper::Private::at_layoutItemChanged(
    const QnResourcePtr& resource, const QnLayoutItemData& item)
{
    if (resource != q->layout())
    {
        NX_ASSERT(resource == q->layout());
        return;
    }

    const auto x = static_cast<int>(item.combinedGeometry.x());
    const auto y = static_cast<int>(item.combinedGeometry.y());
    const auto id = item.resource.id.toString();
    emit q->cameraIdChanged(x, y, id);

    if (QPoint(x, y) == q->displayCell())
        emit q->singleCameraIdChanged();
}

void LiteClientLayoutHelper::Private::at_layoutItemRemoved(
    const QnResourcePtr& resource, const QnLayoutItemData& item)
{
    if (resource != q->layout())
    {
        NX_ASSERT(resource == q->layout());
        return;
    }

    const auto x = static_cast<int>(item.combinedGeometry.x());
    const auto y = static_cast<int>(item.combinedGeometry.y());
    emit q->cameraIdChanged(x, y, QString());

    if (QPoint(x, y) == q->displayCell())
        emit q->singleCameraIdChanged();
}

void LiteClientLayoutHelper::Private::saveLayout()
{
    if (const auto& layout = q->layout())
        qnResourcesChangesManager->saveLayout(layout, [](const QnLayoutResourcePtr&){});
}

} // namespace resource
} // namespace mobile
} // namespace client
} // namespace nx
