#include "resources_messages.h"

#include <QtWidgets/QLabel>

#include <boost/algorithm/cxx11/all_of.hpp>

#include <common/common_module.h>

#include <client_core/client_core_module.h>

#include <client/client_show_once_settings.h>

#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/device_dependent_strings.h>

#include <ui/dialogs/common/message_box.h>
#include <ui/dialogs/common/session_aware_dialog.h>
#include <ui/help/help_topics.h>
#include <ui/widgets/views/resource_list_view.h>

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

namespace nx {
namespace client {
namespace desktop {
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
                return camera->getStatus() != Qn::Offline && !camera->isManuallyAdded();
            });

        text = (cameras.size() == resources.size()
            ? QnDeviceDependentStrings::getNameFromSet(
                qnClientCoreModule->commonModule()->resourcePool(),
                QnCameraDeviceStringSet(
                    tr("Delete %n devices?", "", cameras.size()),
                    tr("Delete %n cameras?", "", cameras.size()),
                    tr("Delete %n I/O Modules?", "", cameras.size())),
                cameras)
            : tr("Delete %n items?", "", resources.size()));

        extras = (onlineAutoDiscoveredCameras.isEmpty()
            ? QString()
            : QnDeviceDependentStrings::getNameFromSet(
                qnClientCoreModule->commonModule()->resourcePool(),
                QnCameraDeviceStringSet(
                    tr("%n of them are auto-discovered.",
                        "", onlineAutoDiscoveredCameras.size()),
                    tr("%n cameras are auto-discovered.",
                        "", onlineAutoDiscoveredCameras.size()),
                    tr("%n I/O modules are auto-discovered.",
                        "", onlineAutoDiscoveredCameras.size())),
                cameras) + L' ' + tr("They may be auto-discovered again after removing."));
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

bool Resources::stopWearableUploads(QWidget* parent, const QnResourceList& resources)
{
    if (resources.isEmpty())
        return true;

    QString text = tr("Video uploading to %n camera(s) will stop:", "", resources.size());
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

} // namespace messages
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
