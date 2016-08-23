#include "layouts_handler_messages.h"

#include <client/client_settings.h>

#include <ui/help/help_topics.h>
#include <ui/widgets/views/resource_list_view.h>

bool QnLayoutsHandlerMessages::changeUserLocalLayout(QWidget* parent, const QnResourceList& stillAccessible)
{
    if (stillAccessible.isEmpty())
        return true;

    /* Check if user have already silenced warning about kept access. */
    if (qnSettings->showOnceMessages().testFlag(Qn::ShowOnceMessage::ChangeUserLocalLayout))
        return true;

    QnMessageBox messageBox(
        QnMessageBox::Warning,
        Qn::Empty_Help,
        tr("Save Layout..."),
        tr("User will keep access to %n removed cameras", "", stillAccessible.size()),
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        parent);
    messageBox.setDefaultButton(QDialogButtonBox::Ok);
    messageBox.setInformativeText(tr("To remove access go to User Settings."));
    messageBox.setCheckBoxText(tr("Do not show this message anymore"));
    messageBox.addCustomWidget(new QnResourceListView(stillAccessible));

    auto result = messageBox.exec();
    if (messageBox.isChecked())
    {
        Qn::ShowOnceMessages messagesFilter = qnSettings->showOnceMessages();
        messagesFilter |= Qn::ShowOnceMessage::ChangeUserLocalLayout;
        qnSettings->setShowOnceMessages(messagesFilter);
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

    QnMessageBox messageBox(
        QnMessageBox::Warning,
        Qn::Empty_Help,
        tr("Save Layout..."),
        tr("All users with this role will get access to these %n cameras", "", toShare.size()),
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        parent);
    messageBox.setDefaultButton(QDialogButtonBox::Ok);
    messageBox.setInformativeText(tr("To remove access go to User Settings."));
    messageBox.setCheckBoxText(tr("Do not show this message anymore"));
    messageBox.addCustomWidget(new QnResourceListView(toShare));

    auto result = messageBox.exec();
    if (messageBox.isChecked())
    {
        Qn::ShowOnceMessages messagesFilter = qnSettings->showOnceMessages();
        messagesFilter |= Qn::ShowOnceMessage::AddToRoleLocalLayout;
        qnSettings->setShowOnceMessages(messagesFilter);
    }

    return result == QDialogButtonBox::Ok;
}

bool QnLayoutsHandlerMessages::removeFromRoleLocalLayout(QWidget* parent, const QnResourceList& stillAccessible)
{
    if (stillAccessible.isEmpty())
        return true;

    /* Check if user have already silenced warning about kept access. */
    if (qnSettings->showOnceMessages().testFlag(Qn::ShowOnceMessage::RemoveFromRoleLocalLayout))
        return true;

    QnMessageBox messageBox(
        QnMessageBox::Warning,
        Qn::Empty_Help,
        tr("Save Layout..."),
        tr("All users with this role will keep access to %n removed cameras", "", stillAccessible.size()),
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        parent);
    messageBox.setDefaultButton(QDialogButtonBox::Ok);
    messageBox.setInformativeText(tr("To remove access go to User Roles Settings."));
    messageBox.setCheckBoxText(tr("Do not show this message anymore"));
    messageBox.addCustomWidget(new QnResourceListView(stillAccessible));

    auto result = messageBox.exec();
    if (messageBox.isChecked())
    {
        Qn::ShowOnceMessages messagesFilter = qnSettings->showOnceMessages();
        messagesFilter |= Qn::ShowOnceMessage::RemoveFromRoleLocalLayout;
        qnSettings->setShowOnceMessages(messagesFilter);
    }

    return result == QDialogButtonBox::Ok;
}

bool QnLayoutsHandlerMessages::sharedLayoutEdit(QWidget* parent)
{
    /* Check if user have already silenced this warning. */
    if (qnSettings->showOnceMessages().testFlag(Qn::ShowOnceMessage::SharedLayoutEdit))
        return true;

    QnMessageBox messageBox(
        QnMessageBox::Warning,
        Qn::Empty_Help,
        tr("Save Layout..."),
        tr("Changes will affect many users"),
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        parent);
    messageBox.setDefaultButton(QDialogButtonBox::Ok);
    messageBox.setInformativeText(tr("This layout is shared. By changing this layout you change it for all users who have it."));
    messageBox.setCheckBoxText(tr("Do not show this message anymore"));

    auto result = messageBox.exec();
    if (messageBox.isChecked())
    {
        Qn::ShowOnceMessages messagesFilter = qnSettings->showOnceMessages();
        messagesFilter |= Qn::ShowOnceMessage::SharedLayoutEdit;
        qnSettings->setShowOnceMessages(messagesFilter);
    }

    return result == QDialogButtonBox::Ok;
}

bool QnLayoutsHandlerMessages::stopSharingLayouts(QWidget* parent,
    const QnResourceList& mediaResources, const QnResourceAccessSubject& subject)
{
    if (mediaResources.isEmpty())
        return true;

    QString informativeText = subject.user()
        ? tr("User will lose access to the following %n cameras:", "", mediaResources.size())
        : tr("All users with this role will lose access to the following %n cameras:", "", mediaResources.size());

    QnMessageBox messageBox(
        QnMessageBox::Warning,
        Qn::Empty_Help,
        tr("Stop Sharing Layout..."),
        tr("If sharing layout is stopped some cameras will become inaccessible"),
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        parent);
    messageBox.setDefaultButton(QDialogButtonBox::Ok);
    messageBox.addCustomWidget(new QnResourceListView(mediaResources));
    messageBox.setInformativeText(informativeText);

    auto result = messageBox.exec();
    return result == QDialogButtonBox::Ok;
}

bool QnLayoutsHandlerMessages::deleteSharedLayouts(QWidget* parent, const QnResourceList& layouts)
{
    QnMessageBox messageBox(
        QnMessageBox::Warning,
        Qn::Empty_Help,
        tr("Delete Layouts..."),
        tr("Changes will affect many users"),
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        parent);
    messageBox.setDefaultButton(QDialogButtonBox::Ok);
    messageBox.setInformativeText(tr("These %n layouts are shared. "
        "By deleting these layouts you delete them from all users who have it.", "", layouts.size()));
    messageBox.addCustomWidget(new QnResourceListView(layouts));
    return messageBox.exec() == QDialogButtonBox::Ok;
}

bool QnLayoutsHandlerMessages::deleteLocalLayouts(QWidget* parent, const QnResourceList& stillAccessible)
{
    if (stillAccessible.isEmpty())
        return true;

    /* Check if user have already silenced warning about kept access. */
    if (qnSettings->showOnceMessages().testFlag(Qn::ShowOnceMessage::DeleteLocalLayouts))
        return true;

    QnMessageBox messageBox(
        QnMessageBox::Warning,
        Qn::Empty_Help,
        tr("Delete Layouts..."),
        tr("User will keep access to %n removed cameras", "", stillAccessible.size()),
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        parent);
    messageBox.setDefaultButton(QDialogButtonBox::Ok);
    messageBox.setInformativeText(tr("To remove access go to User Settings."));
    messageBox.setCheckBoxText(tr("Do not show this message anymore"));
    messageBox.addCustomWidget(new QnResourceListView(stillAccessible));

    auto result = messageBox.exec();
    if (messageBox.isChecked())
    {
        Qn::ShowOnceMessages messagesFilter = qnSettings->showOnceMessages();
        messagesFilter |= Qn::ShowOnceMessage::DeleteLocalLayouts;
        qnSettings->setShowOnceMessages(messagesFilter);
    }

    return result == QDialogButtonBox::Ok;
}
