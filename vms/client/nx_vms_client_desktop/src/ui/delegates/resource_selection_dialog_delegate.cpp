// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_selection_dialog_delegate.h"

#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/desktop/help/help_topic.h>

using namespace nx::vms::client::desktop;

// -------------------------------------------------------------------------- //
// QnResourceSelectionDialogDelegate
// -------------------------------------------------------------------------- //
QnResourceSelectionDialogDelegate::QnResourceSelectionDialogDelegate(
    SystemContext* systemContext,
    QObject* parent)
    :
    QObject(parent),
    SystemContextAware(systemContext)
{
}

QnResourceSelectionDialogDelegate::~QnResourceSelectionDialogDelegate()
{
}

void QnResourceSelectionDialogDelegate::init(QWidget* /*parent*/)
{
}

bool QnResourceSelectionDialogDelegate::validate(const QSet<nx::Uuid>& /*selectedResources*/)
{
    return true;
}

QString QnResourceSelectionDialogDelegate::validationMessage(
    const QSet<nx::Uuid>& /*selectedResources*/) const
{
    return QString();
}

bool QnResourceSelectionDialogDelegate::isValid(const nx::Uuid& /*resource*/) const
{
    return true;
}

QnResourcePtr QnResourceSelectionDialogDelegate::getResource(const nx::Uuid& resource) const
{
    return resourcePool()->getResourceById(resource);
}

QnResourceList QnResourceSelectionDialogDelegate::getResources(const QSet<nx::Uuid>& resources) const
{
    return resourcePool()->getResourcesByIds(resources);
}

bool QnResourceSelectionDialogDelegate::isMultiChoiceAllowed() const
{
    return true;
}
