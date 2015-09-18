#include "resource_selection_dialog_delegate.h"

#include <QtWidgets/QLayout>

#include <core/resource/camera_resource.h>
#include <core/resource/user_resource.h>

#include <ui/models/resource_pool_model.h>


// -------------------------------------------------------------------------- //
// QnResourceSelectionDialogDelegate
// -------------------------------------------------------------------------- //
QnResourceSelectionDialogDelegate::QnResourceSelectionDialogDelegate(QObject *parent):
    QObject(parent)
{

}

QnResourceSelectionDialogDelegate::~QnResourceSelectionDialogDelegate() {

}

void QnResourceSelectionDialogDelegate::init(QWidget* parent) {
    Q_UNUSED(parent)
}

bool QnResourceSelectionDialogDelegate::validate(const QnResourceList &selectedResources) {
    Q_UNUSED(selectedResources)
    return true;
}

bool QnResourceSelectionDialogDelegate::isValid(const QnResourcePtr &resource) const {
    Q_UNUSED(resource)
    return true;
}

QnResourcePoolModelCustomColumnDelegate* QnResourceSelectionDialogDelegate::customColumnDelegate() const {
    return nullptr;
}

bool QnResourceSelectionDialogDelegate::isFlat() const {
    return false;
}

