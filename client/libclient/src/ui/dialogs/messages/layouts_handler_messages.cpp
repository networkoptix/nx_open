#include "layouts_handler_messages.h"

#include <client/client_settings.h>

#include <ui/dialogs/common/session_aware_dialog.h>
#include <ui/help/help_topics.h>
#include <ui/widgets/views/resource_list_view.h>

void QnLayoutsHandlerMessages::layoutAlreadyExists(QWidget* parent)
{
    QnSessionAwareMessageBox::_warning(parent,
        tr("There is another layout with the same name"),
        tr("You don't have permission to overwrite it."));
}

QDialogButtonBox::StandardButton QnLayoutsHandlerMessages::askOverrideLayout(QWidget* parent)
{
    QnSessionAwareMessageBox messageBox(parent);
    messageBox.setIcon(QnMessageBoxIcon::Question);
    messageBox.setText(tr("Overwrite existing layout?"));
    messageBox.setInformativeText(tr("There is another layout with the same name."));
    messageBox.setStandardButtons(QDialogButtonBox::Cancel);
    messageBox._addCustomButton(QnMessageBoxCustomButton::Overwrite);

    return (messageBox.exec() == QDialogButtonBox::Cancel
        ? QDialogButtonBox::Cancel
        : QDialogButtonBox::Yes);
}

bool QnLayoutsHandlerMessages::showCompositeDialog(
    QWidget* parent,
    Qn::ShowOnceMessage showOnceFlag,
    const QString& text,
    const QString& extras,
    const QnResourceList& resources,
    bool useResources)
{
    if (useResources && resources.isEmpty())
        return true;

    /* Check if user have already silenced warning about kept access. */
    const bool useShowOnce = (showOnceFlag != Qn::ShowOnceMessage::Empty);
    if (useShowOnce && qnSettings->showOnceMessages().testFlag(showOnceFlag))
        return true;

    QnSessionAwareMessageBox messageBox(parent);
    messageBox.setIcon(QnMessageBoxIcon::Information);
    messageBox.setText(text);

    if (useResources)
        messageBox.addCustomWidget(new QnResourceListView(resources, true));

    messageBox.setInformativeText(extras);
    messageBox.setCheckBoxText(tr("Don't show this message again"));

    messageBox.setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    messageBox.setDefaultButton(QDialogButtonBox::Ok);

    const auto result = messageBox.exec();

    /**
     * TODO: #ynikitenkov  Very strange logic. Do we have to store
     * answer of dialog to use instead of asking?
     *
     * Moreover. Develop helpers to check and set flags
     */
    if (messageBox.isChecked())
    {
        Qn::ShowOnceMessages messagesFilter = qnSettings->showOnceMessages();
        messagesFilter |= showOnceFlag;
        qnSettings->setShowOnceMessages(messagesFilter);
        qnSettings->save();
    }

    return result == QDialogButtonBox::Ok;
}

bool QnLayoutsHandlerMessages::changeUserLocalLayout(QWidget* parent,
    const QnResourceList& stillAccessible)
{
    return showCompositeDialog(parent, Qn::ShowOnceMessage::ChangeUserLocalLayout,
        tr("User will still have access to %n removed resources:", "", stillAccessible.size()),
        tr("To remove access, please go to User Settings."),
        stillAccessible);
}

bool QnLayoutsHandlerMessages::addToRoleLocalLayout(QWidget* parent, const QnResourceList& toShare)
{
    return showCompositeDialog(parent, Qn::ShowOnceMessage::AddToRoleLocalLayout,
        tr("All users with this role will get access to %n resources:", "", toShare.size()),
        tr("To remove access, please go to Role Settings."),
        toShare);
}

bool QnLayoutsHandlerMessages::removeFromRoleLocalLayout(QWidget* parent,
    const QnResourceList& stillAccessible)
{
    return showCompositeDialog(parent, Qn::ShowOnceMessage::RemoveFromRoleLocalLayout,
        tr("All users with this role will still have access to %n removed resources:",
            "", stillAccessible.size()),
        tr("To remove access, please go to Role Settings."),
        stillAccessible);
}

bool QnLayoutsHandlerMessages::sharedLayoutEdit(QWidget* parent)
{
    static const bool kOmitResources = false;
    return showCompositeDialog(parent, Qn::ShowOnceMessage::SharedLayoutEdit,
        tr("Changes will affect other users"),
        tr("This layout is shared with other users, so you change it for them too."),
        QnResourceList(), kOmitResources);
}

bool QnLayoutsHandlerMessages::stopSharingLayouts(QWidget* parent,
    const QnResourceList& resources, const QnResourceAccessSubject& subject)
{
    // TODO: #ynikitenkov check resources placement
    const QString text = (subject.user()
        ? tr("User will lose access to %n resources:", "", resources.size())
        : tr("All users with this role will lose access to %n resources:", "", resources.size()));

    return showCompositeDialog(parent, Qn::ShowOnceMessage::Empty,
        text, QString(), resources);
}

bool QnLayoutsHandlerMessages::deleteSharedLayouts(QWidget* parent, const QnResourceList& layouts)
{
    // TODO: #ynikitenkov check resources placement, custom button
    QnSessionAwareMessageBox messageBox(parent);
    messageBox.setIcon(QnMessageBoxIcon::Question);
    messageBox.setText(tr("Delete %n shared layouts?", "", layouts.size()));
    messageBox.addCustomWidget(new QnResourceListView(layouts, true));
    messageBox.setInformativeText(
        tr("These %n layouts are shared with other users, so you delete it for them too.",
            "", layouts.size()));
    messageBox.setStandardButtons(QDialogButtonBox::Cancel);
    messageBox._addCustomButton(QnMessageBoxCustomButton::Delete);

    return (messageBox.exec() != QDialogButtonBox::Cancel);
}

bool QnLayoutsHandlerMessages::deleteLocalLayouts(QWidget* parent,
    const QnResourceList& stillAccessible)
{
    return showCompositeDialog(parent, Qn::ShowOnceMessage::DeleteLocalLayouts,
        tr("User will still have access to %n removed resources:", "", stillAccessible.size()),
        tr("To remove access, please go to User Settings."),
        stillAccessible);
}

bool QnLayoutsHandlerMessages::changeVideoWallLayout(QWidget* parent,
    const QnResourceList& inaccessible)
{
    // TODO: #ynikitenkov change description when spec is ready
    const auto extras = tr("You are going to delete some resources to which you have "
        "access from Video Wall only. You won't see them in your resource list after it and won't "
        "be able to add them to Video Wall again.");

    return showCompositeDialog(parent, Qn::ShowOnceMessage::Empty,
        tr("You will lose access to following resources:"),
        extras, inaccessible);
}
