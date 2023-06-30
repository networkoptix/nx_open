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
    const QString systemId;
    QPointer<CloudCrossSystemContext> crossSystemContext;
    nx::utils::ScopedConnection crossSystemContextConnection;
    nx::utils::ScopedConnection systemDescriptionConnection;

    QString text() const
    {
        if (!crossSystemContext)
            return {};

        return toString(crossSystemContext->status());
    }

    bool isVisible() const
    {
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
    d(new Private{
        .systemId = systemId,
        .crossSystemContext = appContext()->cloudCrossSystemManager()->systemContext(systemId)
    })
{

    const auto notifyChanged =
        [this]
        {
            notifyDataChanged({Qt::DisplayRole, core::DecorationPathRole, Qn::FlattenedRole});
        };

    d->crossSystemContextConnection.reset(QObject::connect(
        d->crossSystemContext,
        &CloudCrossSystemContext::statusChanged,
        notifyChanged));

    d->systemDescriptionConnection.reset(QObject::connect(
        d->crossSystemContext->systemDescription(),
        &QnBaseSystemDescription::onlineStateChanged,
        notifyChanged));
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

        case core::DecorationPathRole:
            return d->customIcon();

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
