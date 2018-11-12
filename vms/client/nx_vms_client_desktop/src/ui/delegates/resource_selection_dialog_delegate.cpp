#include "resource_selection_dialog_delegate.h"

#include <core/resource_management/resource_pool.h>

#include <ui/help/help_topics.h>

// -------------------------------------------------------------------------- //
// QnResourceSelectionDialogDelegate
// -------------------------------------------------------------------------- //
QnResourceSelectionDialogDelegate::QnResourceSelectionDialogDelegate(QObject* parent):
    QObject(parent)
{
}

QnResourceSelectionDialogDelegate::~QnResourceSelectionDialogDelegate()
{
}

void QnResourceSelectionDialogDelegate::init(QWidget* /*parent*/)
{
}

bool QnResourceSelectionDialogDelegate::validate(const QSet<QnUuid>& /*selectedResources*/)
{
    return true;
}

QString QnResourceSelectionDialogDelegate::validationMessage(
    const QSet<QnUuid>& /*selectedResources*/) const
{
    return QString();
}

bool QnResourceSelectionDialogDelegate::isValid(const QnUuid& /*resource*/) const
{
    return true;
}

QnResourceTreeModelCustomColumnDelegate* QnResourceSelectionDialogDelegate::customColumnDelegate() const
{
    return nullptr;
}

bool QnResourceSelectionDialogDelegate::isFlat() const
{
    return false;
}

int QnResourceSelectionDialogDelegate::helpTopicId() const
{
    return Qn::Forced_Empty_Help;
}

QnResourcePtr QnResourceSelectionDialogDelegate::getResource(const QnUuid& resource) const
{
    return resourcePool()->getResourceById(resource);
}

QnResourceList QnResourceSelectionDialogDelegate::getResources(const QSet<QnUuid>& resources) const
{
    return resourcePool()->getResourcesByIds(resources);
}

bool QnResourceSelectionDialogDelegate::isMultiChoiceAllowed() const
{
    return true;
}

