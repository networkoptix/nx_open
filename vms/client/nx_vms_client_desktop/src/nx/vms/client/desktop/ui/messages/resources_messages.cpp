// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resources_messages.h"

#include <boost/algorithm/cxx11/all_of.hpp>

#include <QtWidgets/QLabel>

#include <client_core/client_core_module.h>
#include <common/common_module.h>
#include <core/resource/camera_resource.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource/resource.h>
#include <core/resource/webpage_resource.h>
#include <nx/branding.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/settings/show_once_settings.h>
#include <ui/dialogs/common/message_box.h>
#include <ui/dialogs/common/session_aware_dialog.h>
#include <ui/widgets/views/resource_list_view.h>

namespace {

static const QnResourceListView::Options kSimpleOptions(QnResourceListView::HideStatusOption
    | QnResourceListView::ServerAsHealthMonitorOption
    | QnResourceListView::SortAsInTreeOption);

bool showCompositeDialog(
    QWidget* parent,
    nx::utils::property_storage::Property<bool>* showOnceFlag,
    const QString& text,
    const QString& extras,
    const QnResourceList& resources,
    bool useResources = true)
{
    if (useResources && resources.isEmpty())
        return true;

    /* Check if user have already silenced this warning. */
    if (showOnceFlag && showOnceFlag->value())
        return true;

    QnSessionAwareMessageBox messageBox(parent);
    messageBox.setIcon(QnMessageBoxIcon::Information);
    messageBox.setText(text);

    if (useResources)
        messageBox.addCustomWidget(new QnResourceListView(resources, kSimpleOptions, &messageBox));

    messageBox.setInformativeText(extras);
    messageBox.setCheckBoxEnabled(showOnceFlag != nullptr);

    messageBox.setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    messageBox.setDefaultButton(QDialogButtonBox::Ok);

    const auto result = messageBox.exec();

    if (showOnceFlag && messageBox.isChecked())
        showOnceFlag->setValue(true);

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

bool Resources::overrideShowreel(QWidget* parent)
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

bool Resources::sharedLayoutEdit(QWidget* parent)
{
    return showCompositeDialog(parent, &showOnceSettings()->sharedLayoutEdit,
        tr("Changes will affect other users"),
        tr("This layout is shared with other users, so you change it for them too."),
        QnResourceList(), /*useResources*/ false);
}

bool Resources::deleteLayouts(QWidget* parent, const QnResourceList& sharedLayouts,
    const QnResourceList& personalLayouts)
{
    if (sharedLayouts.empty()
        && (personalLayouts.empty() || showOnceSettings()->deleteLocalLayouts()))
    {
        return true;
    }

    QnSessionAwareMessageBox messageBox(parent);
    messageBox.setIcon(QnMessageBoxIcon::Question);
    messageBox.setText(tr("Delete %n layouts?", "", sharedLayouts.size() + personalLayouts.size()));

    if (!sharedLayouts.empty())
    {
        if (personalLayouts.size() == 0)
        {
            messageBox.setInformativeText(
                tr("These layouts are shared with other users, so they will be deleted for their accounts as well.",
                    "Numerical form depends on layouts count",
                    sharedLayouts.size()));
        }
        else
        {
            messageBox.setInformativeText(
                tr("%n layouts are shared with other users, so they will be deleted for their accounts as well.",
                    "",
                    sharedLayouts.size()));
        }
    }

    QnResourceList layouts;
    layouts.append(sharedLayouts);
    layouts.append(personalLayouts);
    messageBox.addCustomWidget(new QnResourceListView(layouts, kSimpleOptions, &messageBox));

    messageBox.setCheckBoxEnabled(sharedLayouts.empty());
    messageBox.setStandardButtons(QDialogButtonBox::Cancel);
    messageBox.addCustomButton(QnMessageBoxCustomButton::Delete,
        QDialogButtonBox::AcceptRole, Qn::ButtonAccent::Warning);

    const auto result = messageBox.exec();

    if (messageBox.isChecked())
        showOnceSettings()->deleteLocalLayouts = true;

    return (result != QDialogButtonBox::Cancel);
}

bool Resources::removeItemsFromLayout(QWidget* parent,
    const QnResourceList& resources)
{
    /* Check if user have already silenced this warning. */
    if (showOnceSettings()->removeItemsFromLayout())
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
        showOnceSettings()->removeItemsFromLayout = true;

    return result != QDialogButtonBox::Cancel;
}

bool Resources::removeItemsFromShowreel(QWidget* parent, const QnResourceList& resources)
{
    /* Check if user have already silenced this warning. Used the same warning as for layouts. */
    if (showOnceSettings()->removeItemsFromLayout())
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
        showOnceSettings()->removeItemsFromLayout = true;

    return result != QDialogButtonBox::Cancel;
}

bool Resources::changeVideoWallLayout(QWidget* parent,
    const QnResourceList& inaccessible)
{
    const auto extras = tr("You are going to delete some resources to which you have "
        "access from Video Wall only. You will not see them in your resource list after it and "
        "will not be able to add them to Video Wall again.");

    return showCompositeDialog(parent, nullptr,
        tr("You will lose access to following resources:"),
        extras, inaccessible);
}

bool Resources::deleteResources(
    QWidget* parent,
    const QnResourceList& resources,
    bool allowSilent)
{
    /* Check if user have already silenced this warning. */
    if (allowSilent)
    {
        if (showOnceSettings()->deleteResources())
            return true;
    }

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
    Qn::ResourceInfoLevel infoLevel = Qn::RI_WithUrl;

    if (usersOnly)
    {
        infoLevel = Qn::RI_NameOnly;
        text = tr("Delete %n users?", "Numerical form depends on users count", resources.size());
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
    messageBox.addCustomWidget(new QnResourceListView(resources, infoLevel, &messageBox));
    if (allowSilent)
        messageBox.setCheckBoxEnabled();

    const auto result = messageBox.exec();

    if (allowSilent)
    {
        if (messageBox.isChecked())
            showOnceSettings()->deleteResources = true;
    }

    return result != QDialogButtonBox::Cancel;
}

bool Resources::deleteResourcesFailed(QWidget* parent, const QnResourceList& resources)
{
    QnSessionAwareMessageBox messageBox(parent);
    messageBox.setIcon(QnMessageBoxIcon::Question);
    messageBox.setText(tr("%n items were not deleted.", "", resources.size()));
    messageBox.setStandardButtons(QDialogButtonBox::Cancel);
    messageBox.addCustomWidget(new QnResourceListView(resources, &messageBox));
    messageBox.setCheckBoxEnabled(false);

    const auto result = messageBox.exec();
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
    if (showOnceSettings()->mergeResourceGroups())
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
        showOnceSettings()->mergeResourceGroups = true;

    return true;
}

bool Resources::moveProxiedWebPages(QWidget* parent, const QnResourceList& resources,
    const QnResourcePtr& server)
{
    static constexpr auto kAnyDomain = "*";

    if (showOnceSettings()->moveProxiedWebpageWarning())
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
        "You are about to move these web pages to Server \"%1\". These web pages proxy all"
             " requested contents, and their proxy server will change to Server \"%1\".",
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
        showOnceSettings()->moveProxiedWebpageWarning = true;

    return result == QDialogButtonBox::AcceptRole;
}

bool Resources::moveCamerasToServer(
        QWidget* parent,
        const QnVirtualCameraResourceList& nonMovableCameras,
        const QString& targetServerName,
        const QString& currentServerName,
        bool hasVirtualCameras,
        bool hasUsbCameras)
{
    if (nonMovableCameras.isEmpty())
        return true;

    QnSessionAwareMessageBox messageBox(parent);

    const QString text = nx::format(
        tr("Only some of the selected devices can be moved to %1"),
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
        tr("Move Partially"),
        QDialogButtonBox::AcceptRole,
        Qn::ButtonAccent::Standard);
    messageBox.addButton(tr("Cancel"), QDialogButtonBox::RejectRole);

    return messageBox.exec() == QDialogButtonBox::AcceptRole;
}

void Resources::warnCamerasCannotBeMoved(
    QWidget* parent,
    bool hasVirtualCameras,
    bool hasUsbCameras)
{
    QString warning;

    if (hasVirtualCameras && hasUsbCameras)
        warning = tr("Virtual cameras, USB or web cameras cannot be moved between servers");
    else if (hasVirtualCameras)
        warning = tr("Virtual cameras cannot be moved between servers");
    else
        warning = tr("USB or web cameras cannot be moved between servers");

    QnMessageBox::warning(parent, warning);
}

} // namespace messages
} // namespace ui
} // namespace nx::vms::client::desktop
