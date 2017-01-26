#include "layouts_handler_messages.h"

#include <client/client_settings.h>

#include <ui/dialogs/common/session_aware_dialog.h>
#include <ui/help/help_topics.h>
#include <ui/widgets/views/resource_list_view.h>

void QnLayoutsHandlerMessages::layoutAlreadyExists(QWidget* parent)
{
    QnSessionAwareMessageBox messageBox(parent);
    messageBox.setIcon(QnMessageBox::Icon::Warning);
    messageBox.setWindowTitle(tr("Layout already exists."));
    messageBox.setText(tr("Layout already exists."));
    messageBox.setInformativeText(tr("A layout with the same name already exists. "
        "You do not have the rights to overwrite it."));
    messageBox.exec();
}

QDialogButtonBox::StandardButton QnLayoutsHandlerMessages::askOverrideLayout(
    QWidget* parent,
    QDialogButtonBox::StandardButtons buttons,
    QDialogButtonBox::StandardButton defaultButton)
{
    QnSessionAwareMessageBox messageBox(parent);
    messageBox.setIcon(QnMessageBox::Icon::Warning);
    messageBox.setWindowTitle(tr("Layout already exists."));
    messageBox.setText(tr("Layout already exists."));
    messageBox.setInformativeText(tr("A layout with the same name already exists. "
        "Would you like to overwrite it?"));
    messageBox.setStandardButtons(buttons);
    messageBox.setDefaultButton(defaultButton);
    return static_cast<QDialogButtonBox::StandardButton>(messageBox.exec());
}

bool QnLayoutsHandlerMessages::changeUserLocalLayout(QWidget* parent,
    const QnResourceList& stillAccessible)
{
    if (stillAccessible.isEmpty())
        return true;

    /* Check if user have already silenced warning about kept access. */
    if (qnSettings->showOnceMessages().testFlag(Qn::ShowOnceMessage::ChangeUserLocalLayout))
        return true;

    QnSessionAwareMessageBox messageBox(parent);
    messageBox.setIcon(QnMessageBox::Icon::Warning);
    messageBox.setWindowTitle(tr("Save Layout..."));
    messageBox.setText(tr("User will keep access to %n removed cameras & resources", "",
        stillAccessible.size()));
    messageBox.setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    messageBox.setDefaultButton(QDialogButtonBox::Ok);
    messageBox.setInformativeText(tr("To remove access go to User Settings."));
    messageBox.setCheckBoxText(tr("Do not show this message anymore"));
    messageBox.addCustomWidget(new QnResourceListView(stillAccessible, true));

    auto result = messageBox.exec();
    if (messageBox.isChecked())
    {
        Qn::ShowOnceMessages messagesFilter = qnSettings->showOnceMessages();
        messagesFilter |= Qn::ShowOnceMessage::ChangeUserLocalLayout;
        qnSettings->setShowOnceMessages(messagesFilter);
        qnSettings->save();
    }

    return result == QDialogButtonBox::Ok;
}

bool QnLayoutsHandlerMessages::addToRoleLocalLayout(QWidget* parent, const QnResourceList& toShare)
{
    if (toShare.isEmpty())
        return true;

    /* Check if user have already silenced warning about kept access. */
    if (qnSettings->showOnceMessages().testFlag(Qn::ShowOnceMessage::AddToRoleLocalLayout))
        return true;

    QnSessionAwareMessageBox messageBox(parent);
    messageBox.setIcon(QnMessageBox::Icon::Warning);
    messageBox.setWindowTitle(tr("Save Layout..."));
    messageBox.setText(tr("All users with this role will get access to these %n cameras & resources", "",
        toShare.size()));
    messageBox.setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    messageBox.setDefaultButton(QDialogButtonBox::Ok);
    messageBox.setInformativeText(tr("To remove access go to User Roles Settings."));
    messageBox.setCheckBoxText(tr("Do not show this message anymore"));
    messageBox.addCustomWidget(new QnResourceListView(toShare, true));

    auto result = messageBox.exec();
    if (messageBox.isChecked())
    {
        Qn::ShowOnceMessages messagesFilter = qnSettings->showOnceMessages();
        messagesFilter |= Qn::ShowOnceMessage::AddToRoleLocalLayout;
        qnSettings->setShowOnceMessages(messagesFilter);
        qnSettings->save();
    }

    return result == QDialogButtonBox::Ok;
}

bool QnLayoutsHandlerMessages::removeFromRoleLocalLayout(QWidget* parent,
    const QnResourceList& stillAccessible)
{
    if (stillAccessible.isEmpty())
        return true;

    /* Check if user have already silenced warning about kept access. */
    if (qnSettings->showOnceMessages().testFlag(Qn::ShowOnceMessage::RemoveFromRoleLocalLayout))
        return true;

    QnSessionAwareMessageBox messageBox(parent);
    messageBox.setIcon(QnMessageBox::Icon::Warning);
    messageBox.setWindowTitle(tr("Save Layout..."));
    messageBox.setText(tr("All users with this role will keep access to %n removed cameras & resources", "",
        stillAccessible.size()));
    messageBox.setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    messageBox.setDefaultButton(QDialogButtonBox::Ok);
    messageBox.setInformativeText(tr("To remove access go to User Roles Settings."));
    messageBox.setCheckBoxText(tr("Do not show this message anymore"));
    messageBox.addCustomWidget(new QnResourceListView(stillAccessible, true));

    auto result = messageBox.exec();
    if (messageBox.isChecked())
    {
        Qn::ShowOnceMessages messagesFilter = qnSettings->showOnceMessages();
        messagesFilter |= Qn::ShowOnceMessage::RemoveFromRoleLocalLayout;
        qnSettings->setShowOnceMessages(messagesFilter);
        qnSettings->save();
    }

    return result == QDialogButtonBox::Ok;
}

bool QnLayoutsHandlerMessages::sharedLayoutEdit(QWidget* parent)
{
    /* Check if user have already silenced this warning. */
    if (qnSettings->showOnceMessages().testFlag(Qn::ShowOnceMessage::SharedLayoutEdit))
        return true;

    QnSessionAwareMessageBox messageBox(parent);
    messageBox.setIcon(QnMessageBox::Icon::Warning);
    messageBox.setWindowTitle(tr("Save Layout..."));
    messageBox.setText(tr("Changes will affect many users"));
    messageBox.setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    messageBox.setDefaultButton(QDialogButtonBox::Ok);
    messageBox.setInformativeText(tr("This layout is shared. "
        "By changing this layout you change it for all users who have it."));
    messageBox.setCheckBoxText(tr("Do not show this message anymore"));

    auto result = messageBox.exec();
    if (messageBox.isChecked())
    {
        Qn::ShowOnceMessages messagesFilter = qnSettings->showOnceMessages();
        messagesFilter |= Qn::ShowOnceMessage::SharedLayoutEdit;
        qnSettings->setShowOnceMessages(messagesFilter);
        qnSettings->save();
    }

    return result == QDialogButtonBox::Ok;
}

bool QnLayoutsHandlerMessages::stopSharingLayouts(QWidget* parent,
    const QnResourceList& mediaResources, const QnResourceAccessSubject& subject)
{
    if (mediaResources.isEmpty())
        return true;

    QString informativeText = subject.user()
        ? tr("User will lose access to the following %n cameras & resources:", "", mediaResources.size())
        : tr("All users with this role will lose access to the following %n cameras & resources:", "",
            mediaResources.size());

    QnSessionAwareMessageBox messageBox(parent);
    messageBox.setIcon(QnMessageBox::Icon::Warning);
    messageBox.setWindowTitle(tr("Stop Sharing Layout..."));
    messageBox.setText(tr("If sharing layout is stopped some cameras & resources will become inaccessible"));
    messageBox.setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    messageBox.setDefaultButton(QDialogButtonBox::Ok);
    messageBox.addCustomWidget(new QnResourceListView(mediaResources, true));
    messageBox.setInformativeText(informativeText);

    auto result = messageBox.exec();
    return result == QDialogButtonBox::Ok;
}

bool QnLayoutsHandlerMessages::deleteSharedLayouts(QWidget* parent, const QnResourceList& layouts)
{
    QnSessionAwareMessageBox messageBox(parent);
    messageBox.setIcon(QnMessageBox::Icon::Warning);
    messageBox.setWindowTitle(tr("Delete Layouts..."));
    messageBox.setText(tr("Changes will affect many users"));
    messageBox.setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    messageBox.setDefaultButton(QDialogButtonBox::Ok);
    messageBox.setInformativeText(tr("These %n layouts are shared. "
        "By deleting these layouts you delete them from all users who have it.", "",
        layouts.size()));
    messageBox.addCustomWidget(new QnResourceListView(layouts, true));
    return messageBox.exec() == QDialogButtonBox::Ok;
}

bool QnLayoutsHandlerMessages::deleteLocalLayouts(QWidget* parent,
    const QnResourceList& stillAccessible)
{
    if (stillAccessible.isEmpty())
        return true;

    /* Check if user have already silenced warning about kept access. */
    if (qnSettings->showOnceMessages().testFlag(Qn::ShowOnceMessage::DeleteLocalLayouts))
        return true;

    QnSessionAwareMessageBox messageBox(parent);
    messageBox.setIcon(QnMessageBox::Icon::Warning);
    messageBox.setWindowTitle(tr("Delete Layouts..."));
    messageBox.setText(tr("User will keep access to %n removed cameras & resources", "",
        stillAccessible.size()));
    messageBox.setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    messageBox.setDefaultButton(QDialogButtonBox::Ok);
    messageBox.setInformativeText(tr("To remove access go to User Settings."));
    messageBox.setCheckBoxText(tr("Do not show this message anymore"));
    messageBox.addCustomWidget(new QnResourceListView(stillAccessible, true));

    auto result = messageBox.exec();
    if (messageBox.isChecked())
    {
        Qn::ShowOnceMessages messagesFilter = qnSettings->showOnceMessages();
        messagesFilter |= Qn::ShowOnceMessage::DeleteLocalLayouts;
        qnSettings->setShowOnceMessages(messagesFilter);
        qnSettings->save();
    }

    return result == QDialogButtonBox::Ok;
}

bool QnLayoutsHandlerMessages::changeVideoWallLayout(QWidget* parent,
    const QnResourceList& inaccessible)
{
    QnSessionAwareMessageBox messageBox(parent);
    messageBox.setIcon(QnMessageBox::Icon::Warning);
    messageBox.setWindowTitle(tr("Update Screen..."));
    messageBox.setText(tr("You will lose access to following resources:"));
    messageBox.setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    messageBox.setDefaultButton(QDialogButtonBox::Ok);
    messageBox.setInformativeText(tr("You are going to delete some resources to which you have "
        "access from videowall only. You won't see them in your resource list after it and won't "
        "be able to add them to videowall again."));
    messageBox.addCustomWidget(new QnResourceListView(inaccessible, true));
    return messageBox.exec() == QDialogButtonBox::Ok;
}
