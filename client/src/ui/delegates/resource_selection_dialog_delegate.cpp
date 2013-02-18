#include "resource_selection_dialog_delegate.h"

#include <business/events/camera_input_business_event.h>
#include <business/events/motion_business_event.h>
#include <business/actions/camera_output_business_action.h>
#include <business/actions/recording_business_action.h>

#include <core/resource/camera_resource.h>
#include <core/resource/user_resource.h>

#include <ui/style/globals.h>

#include <utils/common/email.h>

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


// -------------------------------------------------------------------------- //
// QnCheckResourceAndWarnDelegate
// -------------------------------------------------------------------------- //

template<class ResourceType>
QnCheckResourceAndWarnDelegate<ResourceType>::QnCheckResourceAndWarnDelegate(QWidget* parent):
    QnResourceSelectionDialogDelegate(parent),
    m_warningLabel(NULL)
{
}

template<class ResourceType>
QnCheckResourceAndWarnDelegate<ResourceType>::~QnCheckResourceAndWarnDelegate() {
}


template<class ResourceType>
void QnCheckResourceAndWarnDelegate<ResourceType>::init(QWidget* parent) {
    m_warningLabel = new QLabel(parent);
    QPalette palette = parent->palette();
    palette.setColor(QPalette::WindowText, qnGlobals->errorTextColor());
    m_warningLabel->setPalette(palette);

    parent->layout()->addWidget(m_warningLabel);
}

template<class ResourceType>
bool QnCheckResourceAndWarnDelegate<ResourceType>::validate(const QnResourceList &selected) {
    if (!m_warningLabel)
        return true;
    int invalid = 0;
    QnSharedResourcePointerList<ResourceType> resources = selected.filtered<ResourceType>();
    foreach (const QnSharedResourcePointer<ResourceType> &resource, resources) {
        if (!isResourceValid(resource)) {
            invalid++;
        }
    }
    m_warningLabel->setText(getText(invalid, resources.size()));
    m_warningLabel->setVisible(invalid > 0);
    return true;
}

// -------------------------------------------------------------------------- //
// QnMotionEnabledDelegate
// -------------------------------------------------------------------------- //

QnMotionEnabledDelegate::QnMotionEnabledDelegate(QWidget *parent):
    base_type(parent) {
}

QnMotionEnabledDelegate::~QnMotionEnabledDelegate() {
}

bool QnMotionEnabledDelegate::isResourceValid(const QnVirtualCameraResourcePtr &camera) const {
    return QnMotionBusinessEvent::isResourceValid(camera);
}

QString QnMotionEnabledDelegate::getText(int invalid, int total) const {
    return tr("Recording or motion detection is disabled for %1 of %2 selected cameras.").arg(invalid).arg(total);
}

// -------------------------------------------------------------------------- //
// QnRecordingEnabledDelegate
// -------------------------------------------------------------------------- //

QnRecordingEnabledDelegate::QnRecordingEnabledDelegate(QWidget *parent):
    base_type(parent) {
}

QnRecordingEnabledDelegate::~QnRecordingEnabledDelegate() {
}

bool QnRecordingEnabledDelegate::isResourceValid(const QnVirtualCameraResourcePtr &camera) const {
    return QnRecordingBusinessAction::isResourceValid(camera);
}
QString QnRecordingEnabledDelegate::getText(int invalid, int total) const {
    return tr("Recording is disabled for %1 of %2 selected cameras.").arg(invalid).arg(total);
}

// -------------------------------------------------------------------------- //
// QnInputEnabledDelegate
// -------------------------------------------------------------------------- //

QnInputEnabledDelegate::QnInputEnabledDelegate(QWidget *parent):
    base_type(parent) {
}

QnInputEnabledDelegate::~QnInputEnabledDelegate() {
}

bool QnInputEnabledDelegate::isResourceValid(const QnVirtualCameraResourcePtr &camera) const {
    return QnCameraInputEvent::isResourceValid(camera);
}
QString QnInputEnabledDelegate::getText(int invalid, int total) const {
    return tr("%1 of %2 selected cameras have no input ports.").arg(invalid).arg(total);
}

// -------------------------------------------------------------------------- //
// QnOutputEnabledDelegate
// -------------------------------------------------------------------------- //

QnOutputEnabledDelegate::QnOutputEnabledDelegate(QWidget *parent):
    base_type(parent) {
}

QnOutputEnabledDelegate::~QnOutputEnabledDelegate() {
}

bool QnOutputEnabledDelegate::isResourceValid(const QnVirtualCameraResourcePtr &camera) const {
    return QnCameraOutputBusinessAction::isResourceValid(camera);
}
QString QnOutputEnabledDelegate::getText(int invalid, int total) const {
    return tr("%1 of %2 selected cameras have not output relays.").arg(invalid).arg(total);
}

// -------------------------------------------------------------------------- //
// QnEmailValidDelegate
// -------------------------------------------------------------------------- //

QnEmailValidDelegate::QnEmailValidDelegate(QWidget *parent):
    base_type(parent) {
}

QnEmailValidDelegate::~QnEmailValidDelegate() {
}

bool QnEmailValidDelegate::isResourceValid(const QnUserResourcePtr &user) const {
    return isEmailValid(user->getEmail());
}
QString QnEmailValidDelegate::getText(int invalid, int total) const {
    return tr("%1 of %2 selected users have invalid email.").arg(invalid).arg(total);
}
