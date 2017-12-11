#include "analytics_search_list_model.h"
#include "private/analytics_search_list_model_p.h"

#include <core/resource/camera_resource.h>
#include <ui/help/help_topics.h>
#include <ui/style/skin.h>

#include <nx/vms/event/analytics_helper.h>

namespace nx {
namespace client {
namespace desktop {

AnalyticsSearchListModel::AnalyticsSearchListModel(QObject* parent):
    base_type(parent),
    d(new Private(this))
{
}

AnalyticsSearchListModel::~AnalyticsSearchListModel()
{
}

QnVirtualCameraResourcePtr AnalyticsSearchListModel::camera() const
{
    return d->camera();
}

void AnalyticsSearchListModel::setCamera(const QnVirtualCameraResourcePtr& camera)
{
    d->setCamera(camera);
}

void AnalyticsSearchListModel::setFilterRect(const QRectF& relativeRect)
{
    d->setFilterRect(relativeRect);
}

void AnalyticsSearchListModel::clear()
{
    d->clear();
}

int AnalyticsSearchListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : d->count();
}

QVariant AnalyticsSearchListModel::data(const QModelIndex& index, int role) const
{
    if (!isValid(index))
        return QVariant();

    const auto& object = d->object(index.row());
    NX_ASSERT(!object.track.empty());

    static const auto kDefaultLocale = QString();
    switch (role)
    {
        case Qt::DisplayRole:
            return vms::event::AnalyticsHelper::objectName(d->camera(),
                object.objectTypeId, kDefaultLocale);

        case Qt::DecorationRole:
            return QVariant::fromValue(qnSkin->pixmap(lit("events/analytics.png")));

        case Qn::DescriptionTextRole:
            return d->description(object);

        case Qn::TimestampRole:
        case Qn::PreviewTimeRole:
            return Private::startTimeMs(object);

        case Qn::HelpTopicIdRole:
            return Qn::Empty_Help;

        case Qn::ResourceRole:
            return QVariant::fromValue<QnResourcePtr>(d->camera());

        case Qn::ItemZoomRectRole:
            return QVariant::fromValue(object.track.empty()
                ? QRectF()
                : object.track.front().boundingBox);

        default:
            return base_type::data(index, role);
    }
}

bool AnalyticsSearchListModel::canFetchMore(const QModelIndex& /*parent*/) const
{
    return d->canFetchMore();
}

bool AnalyticsSearchListModel::prefetchAsync(PrefetchCompletionHandler completionHandler)
{
    return d->prefetch(completionHandler);
}

void AnalyticsSearchListModel::commitPrefetch(qint64 keyLimitFromSync)
{
    d->commitPrefetch(keyLimitFromSync);
}

} // namespace
} // namespace client
} // namespace nx
