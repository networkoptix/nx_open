// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cloud_system_status_item.h"

#include <QtCore/QCoreApplication> //< For Q_DECLARE_TR_FUNCTIONS.

#include <client/client_globals.h>
#include <network/base_system_description.h>
#include <nx/utils/scoped_connections.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/cross_system/cloud_cross_system_context.h>
#include <nx/vms/client/desktop/cross_system/cloud_cross_system_manager.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/common/system_context.h>

#include "../../data/resource_tree_globals.h"

namespace nx::vms::client::desktop::entity_resource_tree {

struct CloudSystemStatusItem::Private
{
    Q_DECLARE_TR_FUNCTIONS(CloudSystemStatusItem::Private)

public:
    CloudSystemStatusItem* const q = nullptr;
    const QString systemId;

    // CloudCrossSystemContext is requested when needed.
    mutable QPointer<CloudCrossSystemContext> crossSystemContext;
    mutable nx::utils::ScopedConnections connections;

    void ensureContext() const
    {
        if (!crossSystemContext)
            crossSystemContext = appContext()->cloudCrossSystemManager()->systemContext(systemId);
    }

    QString text() const
    {
        if (appContext()->cloudCrossSystemManager()->isConnectionDeferred(systemId))
            return toString(CloudCrossSystemContext::Status::connecting);

        if (!crossSystemContext)
            return {};

        return toString(crossSystemContext->status());
    }

    bool isVisible() const
    {
        if (appContext()->cloudCrossSystemManager()->isConnectionDeferred(systemId))
            return true;

        if (!crossSystemContext)
            return false;

        switch (crossSystemContext->status())
        {
            case CloudCrossSystemContext::Status::connecting:
                return true;
            case CloudCrossSystemContext::Status::connectionFailure:
                return crossSystemContext->systemDescription()->isOnline() ? true : false;
            default:
                break;
        }
        return ini().crossSystemLayoutsExtendedDebug;
    }

    QString customIcon() const
    {
        if (appContext()->cloudCrossSystemManager()->isConnectionDeferred(systemId))
            return "legacy/loading.gif";

        if (!crossSystemContext)
            return {};

        switch (crossSystemContext->status())
        {
            case CloudCrossSystemContext::Status::uninitialized:
                return "events/alert_red.png";
            case CloudCrossSystemContext::Status::connecting:
                return "legacy/loading.gif";
            case CloudCrossSystemContext::Status::connectionFailure:
                return "events/alert_yellow.png";
            default:
                break;
        }
        return {};
    }

    Qt::ItemFlags flags() const
    {
        if (appContext()->cloudCrossSystemManager()->isConnectionDeferred(systemId))
            return {Qt::ItemIsEnabled};

        if (!crossSystemContext)
            return {};

        switch (crossSystemContext->status())
        {
            case CloudCrossSystemContext::Status::connecting:
                return {Qt::ItemIsEnabled};
            case CloudCrossSystemContext::Status::connectionFailure:
                // Set this flags combination to allow activate item by enter key and single click.
                return {Qt::ItemIsEnabled | Qt::ItemIsSelectable};
            default:
                return {};
        }
    }
};

CloudSystemStatusItem::CloudSystemStatusItem(const QString& systemId):
    base_type(),
    d(new Private{.q = this, .systemId = systemId})
{
    const auto notifyChanged =
        [this]
        {
            notifyDataChanged({Qt::DisplayRole, Qn::DecorationPathRole, Qn::FlattenedRole});
        };

    d->connections << appContext()->cloudCrossSystemManager()->requestSystemContext(
        systemId,
        [this, notifyChanged](CloudCrossSystemContext* context)
        {
            d->crossSystemContext = context;
            d->connections << QObject::connect(
                context, &CloudCrossSystemContext::statusChanged, notifyChanged);
        });

    if (auto* description = appContext()->cloudCrossSystemManager()->systemDescription(systemId))
    {
        d->connections << QObject::connect(
            description,
            &QnBaseSystemDescription::onlineStateChanged,
            notifyChanged);
    }
}

CloudSystemStatusItem::~CloudSystemStatusItem() = default;

QVariant CloudSystemStatusItem::data(int role) const
{
    switch (role)
    {
        case Qt::DisplayRole:
            return d->text();

        case Qt::ToolTipRole:
            return QString();

        case Qn::DecorationPathRole:
        {
            // Currently dynamic creation of items is not supported yet, so use requesting data
            // with Qt::DecorationPathRole as a sign that item is visible/expanded and connect to
            // the system with the higher priority.
            const auto icon = d->customIcon();
            d->ensureContext();
            return icon;
        }

        case Qn::NodeTypeRole:
            return QVariant::fromValue(ResourceTree::NodeType::cloudSystemStatus);

        case Qn::CloudSystemIdRole:
            return d->systemId;

        case Qn::FlattenedRole:
            return !d->isVisible();

        default:
            return {};
    }
}

Qt::ItemFlags CloudSystemStatusItem::flags() const
{
    return d->flags();
}

} // namespace nx::vms::client::desktop::entity_resource_tree
