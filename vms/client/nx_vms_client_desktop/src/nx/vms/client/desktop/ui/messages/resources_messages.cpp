// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resources_messages.h"

#include <QtWidgets/QLabel>

#include <boost/algorithm/cxx11/all_of.hpp>

#include <common/common_module.h>

#include <client_core/client_core_module.h>

#include <client/client_show_once_settings.h>

#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource/webpage_resource.h>

#include <ui/dialogs/common/message_box.h>
#include <ui/dialogs/common/session_aware_dialog.h>
#include <ui/help/help_topics.h>
#include <ui/widgets/views/resource_list_view.h>

#include <nx/branding.h>

namespace {

/* Edit shared layout. */
static const QString kSharedLayoutEditShowOnceKey(lit("SharedLayoutEdit"));

/* Items are removed from user's layout, but access still persist. */
static const QString kChangeUserLocalLayoutShowOnceKey(lit("ChangeUserLocalLayout"));

/* Items are added to roled user's layout. */
static const QString kAddToRoleLocalLayoutShowOnceKey(lit("AddToRoleLocalLayout"));

/* Items are removed from roled user's layout, but access still persist. */
static const QString kRemoveFromRoleLocalLayoutOnceKey(lit("RemoveFromRoleLocalLayout"));

/* Batch delete user's or group's local layouts. */
static const QString kDeleteLocalLayoutsShowOnceKey(lit("DeleteLocalLayouts"));

/* Removing multiple items from layout. */
static const QString kRemoveItemsFromLayoutShowOnceKey(lit("RemoveItemsFromLayout"));

/*  Batch delete resources. */
static const QString kDeleteResourcesShowOnceKey(lit("DeleteResources"));

/*  Merge resource groups. */
static const QString kMergeResourceGroupsShowOnceKey(lit("MergeResourceGroups"));

/* Move proxied webpages to another server. */
static const QString kMoveProxiedWebpageWarningShowOnceKey("MoveProxiedWebpageWarning");

static const QnResourceListView::Options kSimpleOptions(QnResourceListView::HideStatusOption
    | QnResourceListView::ServerAsHealthMonitorOption
    | QnResourceListView::SortAsInTreeOption);

bool showCompositeDialog(
    QWidget* parent,
    const QString& showOnceFlag,
    const QString& text,
    const QString& extras,
    const QnResourceList& resources,
    bool useResources = true)
{
    if (useResources && resources.isEmpty())
        return true;

    /* Check if user have already silenced this warning. */
    if (!showOnceFlag.isEmpty() && qnClientShowOnce->testFlag(showOnceFlag))
        return true;

    QnSessionAwareMessageBox messageBox(parent);
    messageBox.setIcon(QnMessageBoxIcon::Information);
    messageBox.setText(text);

    if (useResources)
        messageBox.addCustomWidget(new QnResourceListView(resources, kSimpleOptions, &messageBox));

    messageBox.setInformativeText(extras);
    messageBox.setCheckBoxEnabled(!showOnceFlag.isEmpty());

    messageBox.setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    messageBox.setDefaultButton(QDialogButtonBox::Ok);

    const auto result = messageBox.exec();

    if (!showOnceFlag.isEmpty() && messageBox.isChecked())
        qnClientShowOnce->setFlag(showOnceFlag);

    return result == QDialogButtonBox::Ok;
}

} // namespace

namespace nx::vms::client::desktop {
namespace ui {
namespace messages {

void Resources::layoutAlreadyExists(QWidget* parent)
{
    QnSessionAwareMessageBox::warning(parent,
        tr("There is another layout with the same name"),
        tr("You do not have permission to overwrite it."));
}

bool Resources::overrideLayout(QWidget* parent)
{
    QnSessionAwareMessageBox messageBox(parent);
    messageBox.setIcon(QnMessageBoxIcon::Question);
    messageBox.setText(tr("Overwrite existing layout?"));
    messageBox.setInformativeText(tr("There is another layout with the same name."));
    messageBox.setStandardButtons(QDialogButtonBox::Cancel);
    messageBox.addCustomButton(QnMessageBoxCustomButton::Overwrite,
        QDialogButtonBox::AcceptRole, Qn::ButtonAccent::Warning);
    return messageBox.exec() != QDialogButtonBox::Cancel;
}

bool Resources::overrideLayoutTour(QWidget* parent)
{
    QnSessionAwareMessageBox messageBox(parent);
    messageBox.setIcon(QnMessageBoxIcon::Question);
    messageBox.setText(tr("Overwrite existing showreel?"));
    messageBox.setInformativeText(tr("There is another showreel with the same name."));
    messageBox.setStandardButtons(QDialogButtonBox::Cancel);
    messageBox.addCustomButton(QnMessageBoxCustomButton::Overwrite,
        QDialogButtonBox::AcceptRole, Qn::ButtonAccent::Warning);
    return messageBox.exec() != QDialogButtonBox::Cancel;
}

bool Resources::changeUserLocalLayout(QWidget* parent,
    const QnResourceList& stillAccessible)
{
    return showCompositeDialog(parent, kChangeUserLocalLayoutShowOnceKey,
        tr("User will still have access to %n removed resources:", "", stillAccessible.size()),
        tr("To remove access, please go to User Settings."),
        stillAccessible);
}

bool Resources::addToRoleLocalLayout(QWidget* parent, const QnResourceList& toShare)
{
    return showCompositeDialog(parent, kAddToRoleLocalLayoutShowOnceKey,
        tr("All users with this role will get access to %n resources:", "", toShare.size()),
        tr("To remove access, please go to Role Settings."),
        toShare);
}

bool Resources::removeFromRoleLocalLayout(QWidget* parent,
    const QnResourceList& stillAccessible)
{
    return showCompositeDialog(parent, kRemoveFromRoleLocalLayoutOnceKey,
        tr("All users with this role will still have access to %n removed resources:",
            "", stillAccessible.size()),
        tr("To remove access, please go to Role Settings."),
        stillAccessible);
}

bool Resources::sharedLayoutEdit(QWidget* parent)
{
    return showCompositeDialog(parent, kSharedLayoutEditShowOnceKey,
        tr("Changes will affect other users"),
        tr("This layout is shared with other users, so you change it for them too."),
        QnResourceList(), /*useResources*/ false);
}

bool Resources::stopSharingLayouts(QWidget* parent,
    const QnResourceList& resources, const QnResourceAccessSubject& subject)
{
    const QString text = (subject.user()
        ? tr("User will lose access to %n resources:", "", resources.size())
        : tr("All users with this role will lose access to %n resources:", "", resources.size()));

    return showCompositeDialog(parent, QString(), text, QString(), resources);
}

bool Resources::deleteSharedLayouts(QWidget* parent, const QnResourceList& layouts)
{
    QnSessionAwareMessageBox messageBox(parent);
    messageBox.setIcon(QnMessageBoxIcon::Question);
    messageBox.setText(tr("Delete %n shared layouts?", "", layouts.size()));
    messageBox.addCustomWidget(new QnResourceListView(layouts, kSimpleOptions, &messageBox));
    messageBox.setInformativeText(
        tr("These %n layouts are shared with other users, so you delete it for them too.",
            "", layouts.size()));
    messageBox.setStandardButtons(QDialogButtonBox::Cancel);
    messageBox.addCustomButton(QnMessageBoxCustomButton::Delete,
        QDialogButtonBox::AcceptRole, Qn::ButtonAccent::Warning);

    return (messageBox.exec() != QDialogButtonBox::Cancel);
}

bool Resources::deleteLocalLayouts(QWidget* parent,
    const QnResourceList& stillAccessible)
{
    return showCompositeDialog(parent, kDeleteLocalLayoutsShowOnceKey,
        tr("User will still have access to %n removed resources:", "", stillAccessible.size()),
        tr("To remove access, please go to User Settings."),
        stillAccessible);
}

bool Resources::removeItemsFromLayout(QWidget* parent,
    const QnResourceList& resources)
{
    /* Check if user have already silenced this warning. */
    if (qnClientShowOnce->testFlag(kRemoveItemsFromLayoutShowOnceKey))
        return true;

    QnSessionAwareMessageBox messageBox(parent);
    messageBox.setIcon(QnMessageBoxIcon::Question);
    messageBox.setText(tr("Remove %n items from layout?", "", resources.size()));
    messageBox.setStandardButtons(QDialogButtonBox::Cancel);
    messageBox.addButton(tr("Remove"), QDialogButtonBox::AcceptRole, Qn::ButtonAccent::Warning);
    messageBox.addCustomWidget(new QnResourceListView(resources, kSimpleOptions, &messageBox));
    messageBox.setCheckBoxEnabled();
    const auto result = messageBox.exec();
    if (messageBox.isChecked())
        qnClientShowOnce->setFlag(kRemoveItemsFromLayoutShowOnceKey);

    return result != QDialogButtonBox::Cancel;
}

bool Resources::removeItemsFromLayoutTour(QWidget* parent, const QnResourceList& resources)
{
    /* Check if user have already silenced this warning. */
    if (qnClientShowOnce->testFlag(kRemoveItemsFromLayoutShowOnceKey))
        return true;

    QnSessionAwareMessageBox messageBox(parent);
    messageBox.setIcon(QnMessageBoxIcon::Question);
    messageBox.setText(tr("Remove %n items from showreel?", "", resources.size()));
    messageBox.setStandardButtons(QDialogButtonBox::Cancel);
    messageBox.addButton(tr("Remove"), QDialogButtonBox::AcceptRole, Qn::ButtonAccent::Warning);
    messageBox.addCustomWidget(new QnResourceListView(resources, kSimpleOptions, &messageBox));
    messageBox.setCheckBoxEnabled();
    const auto result = messageBox.exec();
    if (messageBox.isChecked())
        qnClientShowOnce->setFlag(kRemoveItemsFromLayoutShowOnceKey);

    return result != QDialogButtonBox::Cancel;
}

bool Resources::changeVideoWallLayout(QWidget* parent,
    const QnResourceList& inaccessible)
{
    const auto extras = tr("You are going to delete some resources to which you have "
        "access from Video Wall only. You will not see them in your resource list after it and "
        "will not be able to add them to Video Wall again.");

    return showCompositeDialog(parent, QString(),
        tr("You will lose access to following resources:"),
        extras, inaccessible);
}

bool Resources::deleteResources(QWidget* parent, const QnResourceList& resources)
{
    /* Check if user have already silenced this warning. */
    if (qnClientShowOnce->testFlag(kDeleteResourcesShowOnceKey))
        return true;

    if (resources.isEmpty())
        return true;

    auto checkFlag = [resources](Qn::ResourceFlag flag)
        {
            return boost::algorithm::all_of(resources,
                [flag](const QnResourcePtr& resource)
                {
                    return resource->hasFlags(flag);
                });
        };

    const bool usersOnly = checkFlag(Qn::user);

    QString text;
    QString extras;

    if (usersOnly)
    {
        text = tr("Delete %n users?", "", resources.size());
    }
    else
    {
        const auto cameras = resources.filtered<QnVirtualCameraResource>();

        /* Check that we are deleting online auto-found cameras */
        const auto onlineAutoDiscoveredCameras = cameras.filtered(
            [](const QnVirtualCameraResourcePtr& camera)
            {
                return camera->getStatus() != nx::vms::api::ResourceStatus::offline
                    && !camera->isManuallyAdded();
            });

        text = (cameras.size() == resources.size()
            ? QnDeviceDependentStrings::getNameFromSet(
                qnClientCoreModule->resourcePool(),
                QnCameraDeviceStringSet(
                    tr("Delete %n devices?", "", cameras.size()),
                    tr("Delete %n cameras?", "", cameras.size()),
                    tr("Delete %n I/O Modules?", "", cameras.size())),
                cameras)
            : tr("Delete %n items?", "", resources.size()));

        extras = (onlineAutoDiscoveredCameras.isEmpty()
            ? QString()
            : QnDeviceDependentStrings::getNameFromSet(
                qnClientCoreModule->resourcePool(),
                QnCameraDeviceStringSet(
                    tr("%n of them are auto-discovered.",
                        "", onlineAutoDiscoveredCameras.size()),
                    tr("%n cameras are auto-discovered.",
                        "", onlineAutoDiscoveredCameras.size()),
                    tr("%n I/O modules are auto-discovered.",
                        "", onlineAutoDiscoveredCameras.size())),
                cameras) + ' ' + tr("They may be auto-discovered again after removing."));
    }

    QnSessionAwareMessageBox messageBox(parent);
    messageBox.setIcon(QnMessageBoxIcon::Question);
    messageBox.setText(text);
    messageBox.setInformativeText(extras);
    messageBox.setStandardButtons(QDialogButtonBox::Cancel);
    messageBox.addCustomButton(QnMessageBoxCustomButton::Delete,
        QDialogButtonBox::AcceptRole, Qn::ButtonAccent::Warning);
    messageBox.addCustomWidget(new QnResourceListView(resources, &messageBox));
    messageBox.setCheckBoxEnabled();

    const auto result = messageBox.exec();
    if (messageBox.isChecked())
        qnClientShowOnce->setFlag(kDeleteResourcesShowOnceKey);

    return result != QDialogButtonBox::Cancel;
}

bool Resources::stopVirtualCameraUploads(QWidget* parent, const QnResourceList& resources)
{
    if (resources.isEmpty())
        return true;

    QString text = tr("Some video files are still being uploaded to %n virtual cameras:",
        "", resources.size());
    QString extra = tr("Stop uploading?");

    QnSessionAwareMessageBox messageBox(parent);
    messageBox.setIcon(QnMessageBoxIcon::Question);
    messageBox.setText(text);
    messageBox.setStandardButtons(QDialogButtonBox::Cancel);
    messageBox.addButton(tr("Stop"), QDialogButtonBox::AcceptRole, Qn::ButtonAccent::Warning);
    messageBox.addCustomWidget(new QnResourceListView(resources, &messageBox));
    messageBox.addCustomWidget(new QLabel(extra, &messageBox));

    return messageBox.exec() != QDialogButtonBox::Cancel;
}

bool Resources::mergeResourceGroups(QWidget* parent, const QString& groupName)
{
    if (qnClientShowOnce->testFlag(kMergeResourceGroupsShowOnceKey))
        return true;

    QnSessionAwareMessageBox messageBox(parent);
    messageBox.setIcon(QnMessageBoxIcon::Question);
    messageBox.setText(tr("Merge this group with %1?").arg(groupName));
    messageBox.setInformativeText(tr("Two groups with the same name cannot exist."));
    messageBox.setStandardButtons(QDialogButtonBox::Cancel);
    messageBox.addButton(tr("Merge"), QDialogButtonBox::AcceptRole, Qn::ButtonAccent::Standard);
    messageBox.setCheckBoxEnabled();

    const auto result = messageBox.exec();
    if (result == QDialogButtonBox::Cancel)
        return false;

    if (messageBox.isChecked())
        qnClientShowOnce->setFlag(kMergeResourceGroupsShowOnceKey);

    return true;
}

bool Resources::moveProxiedWebPages(QWidget* parent, const QnResourceList& resources,
    const QnResourcePtr& server)
{
    static constexpr auto kAnyDomain = "*";

    const QString showOnceFlag = kMoveProxiedWebpageWarningShowOnceKey;

    if (qnClientShowOnce->testFlag(showOnceFlag))
        return true;

    const auto webPages = resources.filtered<QnWebPageResource>();

    if (webPages.count() == 0 || !server)
        return true;

    QnResourceList allContentsWebpages;
    for (const auto& webPage: webPages)
    {
        if (webPage->getProxyId() != server->getId()
            && webPage->proxyDomainAllowList().contains(kAnyDomain))
        {
            allContentsWebpages << webPage;
        }
    }

    if (allContentsWebpages.isEmpty())
        return true;

    QnSessionAwareMessageBox messageBox(parent);

    const bool multipleWebpagesAreMoved = webPages.count() > 1;

    const auto text = tr(
        "You are about to move these webpages to server \"%1\". These webpages proxy all"
             " requested contents, and their proxy server will change to server \"%1\".",
        "",
        webPages.count());

    messageBox.setWindowTitle(nx::branding::vmsName());
    messageBox.setIcon(QnMessageBoxIcon::Warning);
    messageBox.setText(text.arg(server->getName()));
    messageBox.setInformativeText(
        nx::format("%1\n\n%2",
            tr("Proxying all contents exposes any service or device on the server's network to the"
               " users of this webpage."),
            tr("Move anyway?")
        ));

    // Always show a list of webpages when multiple webpages are moved, even if only one of them
    // proxies all contents.
    if (webPages.count() > 1)
    {
        messageBox.addCustomWidget(
            new QnResourceListView(allContentsWebpages, kSimpleOptions, &messageBox),
            QnMessageBox::Layout::AfterMainLabel);
    }

    messageBox.setCheckBoxEnabled(true);

    messageBox.addButton(tr("Move"), QDialogButtonBox::AcceptRole, Qn::ButtonAccent::Standard);
    messageBox.addButton(tr("Cancel"), QDialogButtonBox::RejectRole);

    const auto result = messageBox.exec();

    if (messageBox.isChecked())
        qnClientShowOnce->setFlag(showOnceFlag);

    return result == QDialogButtonBox::AcceptRole;
}

bool Resources::moveCamerasToServer(
        QWidget* parent,
        const QnVirtualCameraResourceList& nonMovableCameras,
        const QString& groupName,
        const QString& targetServerName,
        const QString& currentServerName,
        bool hasVirtualCameras,
        bool hasUsbCameras)
{
    if (nonMovableCameras.isEmpty())
        return true;

    QnSessionAwareMessageBox messageBox(parent);

    const QString text = nx::format(
        tr("Some devices from %1 will not be moved to %2. Move anyways?"),
        groupName,
        targetServerName);

    QString infoText;

    if (hasVirtualCameras && hasUsbCameras)
        infoText = tr("Virtual cameras, USB or web cameras cannot be moved between servers. These devices will remain on %1:");
    else if (hasUsbCameras)
        infoText = tr("USB or web cameras cannot be moved between servers. These devices will remain on %1:");
    else
        infoText = tr("Virtual cameras cannot be moved between servers. These devices will remain on %1:");

    infoText = infoText.arg(currentServerName);

    messageBox.setWindowTitle(nx::branding::vmsName());
    messageBox.setIcon(QnMessageBoxIcon::Question);
    messageBox.setText(text);
    messageBox.setInformativeText(infoText);

    messageBox.addCustomWidget(
        new QnResourceListView(nonMovableCameras, kSimpleOptions, &messageBox),
        QnMessageBox::Layout::Content);

    messageBox.addButton(
        tr("Move Group without them"),
        QDialogButtonBox::AcceptRole,
        Qn::ButtonAccent::Standard);
    messageBox.addButton(tr("Cancel"), QDialogButtonBox::RejectRole);

    return messageBox.exec() == QDialogButtonBox::AcceptRole;
}

void Resources::warnCamerasCannotBeMoved(
    QWidget* parent,
    const QString& groupName,
    bool hasVirtualCameras,
    bool hasUsbCameras)
{
    QString warning;

    if (hasVirtualCameras && hasUsbCameras)
    {
        warning = groupName.isEmpty()
            ? tr("Virtual cameras, USB or web cameras cannot be moved between servers")
            : tr("%1 cannot be moved between servers as it contains "
                "virtual cameras, USB or web cameras").arg(groupName);
    }
    else if (hasVirtualCameras)
    {
        warning = groupName.isEmpty()
            ? tr("Virtual cameras cannot be moved between servers")
            : tr("%1 cannot be moved between servers as it contains "
                "virtual cameras").arg(groupName);
    }
    else
    {
        warning = groupName.isEmpty()
            ? tr("USB or web cameras cannot be moved between servers")
            : tr("%1 cannot be moved between servers as it contains "
                "USB or web cameras").arg(groupName);
    }

    QnMessageBox::warning(parent, warning);
}

} // namespace messages
} // namespace ui
} // namespace nx::vms::client::desktop
