// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cloud_system_status_item.h"

#include <QtCore/QCoreApplication> //< For Q_DECLARE_TR_FUNCTIONS.

#include <client/client_globals.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/cross_system/cloud_cross_system_context.h>
#include <nx/vms/client/desktop/cross_system/cloud_cross_system_manager.h>
#include <nx/vms/client/desktop/style/skin.h>
#include <nx/vms/common/system_context.h>

#include "../../data/resource_tree_globals.h"

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

struct CloudSystemStatusItem::Private
{
    Q_DECLARE_TR_FUNCTIONS(CloudSystemStatusItem::Private)

public:
    const CloudSystemStatusItem* q;
    const QString systemId;
    QPointer<CloudCrossSystemContext> systemContext;
    nx::utils::ScopedConnections connections;

    CloudCrossSystemContext::Status status() const
    {
        return NX_ASSERT(systemContext)
            ? systemContext->status()
            : CloudCrossSystemContext::Status::uninitialized;
    }

    QString text() const
    {
        switch (status())
        {
            case CloudCrossSystemContext::Status::uninitialized:
                return "UNINITIALIZED"; //< Debug purposes.
            case CloudCrossSystemContext::Status::connecting:
                return tr("Loading...");
            case CloudCrossSystemContext::Status::connectionFailure:
                return tr("User interaction required");
            case CloudCrossSystemContext::Status::unsupported:
                return "UNSUPPORTED"; //< Debug purposes.
            case CloudCrossSystemContext::Status::connected:
                return "CONNECTED"; //< Debug purposes.
            default:
                break;
        }
        return QString();
    }

    bool isVisible() const
    {
        switch (status())
        {
            case CloudCrossSystemContext::Status::connecting:
            case CloudCrossSystemContext::Status::connectionFailure:
                return true;
            default:
                break;
        }
        return false;
    }

    QString customIcon() const
    {
        switch (status())
        {
            case CloudCrossSystemContext::Status::connecting:
                return "legacy/loading.gif";
            case CloudCrossSystemContext::Status::connectionFailure:
                return "events/alert_yellow.png";
            default:
                break;
        }
        return QString();
    }
};

CloudSystemStatusItem::CloudSystemStatusItem(const QString& systemId):
    base_type(),
    d(new Private{
        .q = this,
        .systemId = systemId,
        .systemContext = appContext()->cloudCrossSystemManager()->systemContext(systemId)
    })
{
    if (!NX_ASSERT(d->systemContext))
        return;

    NX_VERBOSE(this, "Connect to %1", *d->systemContext);

    d->connections << QObject::connect(d->systemContext.data(),
        &CloudCrossSystemContext::statusChanged,
        [this]()
        {
            NX_VERBOSE(this, "%1 status changed to %2", *d->systemContext, d->text());
            notifyDataChanged({Qt::DisplayRole, Qn::DecorationPathRole, Qn::FlattenedRole});
        });
}

CloudSystemStatusItem::~CloudSystemStatusItem()
{
}

QVariant CloudSystemStatusItem::data(int role) const
{
    switch (role)
    {
        case Qt::DisplayRole:
            return d->text();

        case Qt::ToolTipRole:
            return QString();

        case Qn::DecorationPathRole:
            return d->customIcon();

        case Qn::NodeTypeRole:
            return QVariant::fromValue(ResourceTree::NodeType::cloudSystemStatus);

        case Qn::CloudSystemIdRole:
            return d->systemId;

        case Qn::FlattenedRole:
            return !d->isVisible();
    }
    return QVariant();
}

Qt::ItemFlags CloudSystemStatusItem::flags() const
{
    return {Qt::ItemIsEnabled};
}

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
