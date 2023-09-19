// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_selection_dialog_delegate.h"

#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/desktop/help/help_topic.h>

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
