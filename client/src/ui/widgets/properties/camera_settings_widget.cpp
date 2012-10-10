#include "camera_settings_widget.h"

#include <QtGui/QLabel>
#include <QtGui/QStackedWidget>
#include <QtGui/QVBoxLayout>

#include <core/resource/camera_resource.h>
#include <core/resource_managment/resource_criterion.h>

#include <ui/actions/action_manager.h>
#include <ui/workbench/workbench_context.h>

#include "multiple_camera_settings_widget.h"
#include "single_camera_settings_widget.h"

QnCameraSettingsWidget::QnCameraSettingsWidget(QWidget *parent, QnWorkbenchContext *context): 
    QWidget(parent),
    QnWorkbenchContextAware(context ? static_cast<QObject *>(context) : parent),
    m_emptyTab(Qn::GeneralSettingsTab)
{
    /* Create per-mode widgets. */
    QLabel *invalidWidget = new QLabel(tr("Cannot edit properties for items of different types."), this);
    invalidWidget->setAlignment(Qt::AlignCenter);
    m_invalidWidget = invalidWidget;

    QLabel *emptyWidget = new QLabel(tr("No cameras selected."), this);
    emptyWidget->setAlignment(Qt::AlignCenter);
    m_emptyWidget = emptyWidget;

    m_multiWidget = new QnMultipleCameraSettingsWidget(this);

    m_singleWidget = new QnSingleCameraSettingsWidget(this);

    connect(m_multiWidget, SIGNAL(moreLicensesRequested()), this, SLOT(at_moreLicensesRequested()));
    connect(m_singleWidget, SIGNAL(moreLicensesRequested()), this, SLOT(at_moreLicensesRequested()));
    connect(m_singleWidget, SIGNAL(advancedSettingChanged()), this, SLOT(at_advancedSettingChanged()));

    /* Stack per-mode widgets. */
    m_stackedWidget = new QStackedWidget(this);
    m_stackedWidget->insertWidget(InvalidMode, m_invalidWidget);
    m_stackedWidget->insertWidget(EmptyMode, m_emptyWidget);
    m_stackedWidget->insertWidget(SingleMode, m_singleWidget);
    m_stackedWidget->insertWidget(MultiMode, m_multiWidget);
    m_stackedWidget->setCurrentIndex(EmptyMode);

    /* Init this widget. */
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_stackedWidget);
}

QnCameraSettingsWidget::~QnCameraSettingsWidget() {
    return;
}

QnCameraSettingsWidget::Mode QnCameraSettingsWidget::mode() const {
    return static_cast<Mode>(m_stackedWidget->currentIndex());
}

const QnResourceList &QnCameraSettingsWidget::resources() const {
    return m_resources;
}

const QnVirtualCameraResourceList &QnCameraSettingsWidget::cameras() const {
    return m_cameras;
}

void QnCameraSettingsWidget::setResources(const QnResourceList &resources) {
    m_resources = resources.toSet().toList();
    m_cameras = m_resources.filtered<QnVirtualCameraResource>();

    if(m_cameras.size() != m_resources.size() && m_cameras.size() != 0) {
        setMode(InvalidMode);
    } else {
        switch(m_cameras.size()) {
        case 0: 
            setMode(EmptyMode);
            break;
        case 1: 
            m_singleWidget->setCamera(m_cameras.front());
            setMode(SingleMode);
            break;
        default: 
            m_multiWidget->setCameras(m_cameras);
            setMode(MultiMode);
            break;
        }
    }
}

Qn::CameraSettingsTab QnCameraSettingsWidget::currentTab() const {
    switch(mode()) {
    case SingleMode: return m_singleWidget->currentTab();
    case MultiMode: return m_multiWidget->currentTab();
    default: return m_emptyTab;
    }
}

void QnCameraSettingsWidget::setCurrentTab(Mode mode, Qn::CameraSettingsTab tab) {
    switch(mode) {
    case SingleMode: 
        m_singleWidget->setCurrentTab(tab);
        break;
    case MultiMode:
        m_multiWidget->setCurrentTab(tab);
        break;
    default:
        m_emptyTab = tab;
        break;
    }
}

void QnCameraSettingsWidget::setCurrentTab(Qn::CameraSettingsTab tab) {
    setCurrentTab(mode(), tab);
}

int QnCameraSettingsWidget::activeCameraCount() const {
    switch(mode()) {
    case SingleMode:
        return m_singleWidget->isCameraActive() ? 1 : 0;
    case MultiMode:
        return m_multiWidget->activeCameraCount();
    default:
        return 0;
    }
}

void QnCameraSettingsWidget::setCamerasActive(bool active) {
    switch(mode()) {
    case SingleMode:
        m_singleWidget->setCameraActive(active);
        break;
    case MultiMode:
        m_multiWidget->setCamerasActive(active);
        break;
    default:
        break;
    }
}

bool QnCameraSettingsWidget::hasDbChanges() const {
    switch(mode()) {
    case SingleMode: return m_singleWidget->hasDbChanges();
    case MultiMode: return m_multiWidget->hasDbChanges();
    default: return false;
    }
}

bool QnCameraSettingsWidget::hasCameraChanges() const {
    switch(mode()) {
    case SingleMode: return m_singleWidget->hasCameraChanges();
    case MultiMode: return false;
    default: return false;
    }
}

bool QnCameraSettingsWidget::hasAnyCameraChanges() const {
    switch(mode()) {
    case SingleMode: return m_singleWidget->hasAnyCameraChanges();
    case MultiMode: return false;
    default: return false;
    }
}

bool QnCameraSettingsWidget::hasControlsChanges() const {
    switch(mode()) {
    case SingleMode: return m_singleWidget->hasControlsChanges();
    case MultiMode: return false; //TODO: gdm
    default: return false;
    }
}

const QList< QPair< QString, QVariant> >& QnCameraSettingsWidget::getModifiedAdvancedParams() const
{
    switch(mode()) {
    case SingleMode: return m_singleWidget->getModifiedAdvancedParams();
    case MultiMode: return m_multiWidget->getModifiedAdvancedParams();
    default: Q_ASSERT(false); return *(new QList< QPair< QString, QVariant> >());
    }
}

QnMediaServerConnectionPtr QnCameraSettingsWidget::getServerConnection() const
{
    switch(mode()) {
    case SingleMode: return m_singleWidget->getServerConnection();
    case MultiMode: return m_multiWidget->getServerConnection();
    default: return QnMediaServerConnectionPtr(0);
    }
}

bool QnCameraSettingsWidget::isReadOnly() const {
    return m_singleWidget->isReadOnly();
}

void QnCameraSettingsWidget::setReadOnly(bool readOnly) const {
    m_singleWidget->setReadOnly(readOnly);
    m_multiWidget->setReadOnly(readOnly);
}

void QnCameraSettingsWidget::updateFromResources() {
    switch(mode()) {
    case SingleMode:
        m_singleWidget->updateFromResource();
        break;
    case MultiMode:
        m_multiWidget->updateFromResources();
        break;
    default:
        break;
    }
}

void QnCameraSettingsWidget::submitToResources() {
    switch(mode()) {
    case SingleMode:
        m_singleWidget->submitToResource();
        break;
    case MultiMode:
        m_multiWidget->submitToResources();
        break;
    default:
        break;
    }
}

void QnCameraSettingsWidget::setMode(Mode mode) {
    Mode oldMode = this->mode();
    if(mode == oldMode)
        return;

    bool oldHasChanges = hasDbChanges() || hasCameraChanges();
    Qn::CameraSettingsTab oldTab = currentTab();

    if(m_stackedWidget->currentIndex() == SingleMode || m_stackedWidget->currentIndex() == MultiMode)
        disconnect(m_stackedWidget->currentWidget(), SIGNAL(hasChangesChanged()), this, SIGNAL(hasChangesChanged()));

    setCurrentTab(mode, oldTab);

    switch(oldMode) {
    case SingleMode:
        m_singleWidget->setCamera(QnVirtualCameraResourcePtr());
        break;
    case MultiMode:
        m_multiWidget->setCameras(QnVirtualCameraResourceList());
        break;
    default:
        break;
    }

    m_stackedWidget->setCurrentIndex(mode);

    if(m_stackedWidget->currentIndex() == SingleMode || m_stackedWidget->currentIndex() == MultiMode)
        connect(m_stackedWidget->currentWidget(), SIGNAL(hasChangesChanged()), this, SIGNAL(hasChangesChanged()));

    bool newHasChanges = hasDbChanges() || hasCameraChanges();
    if(oldHasChanges != newHasChanges)
        emit hasChangesChanged();

    emit modeChanged();
}

void QnCameraSettingsWidget::at_moreLicensesRequested() {
    menu()->trigger(Qn::GetMoreLicensesAction);
}

void QnCameraSettingsWidget::at_advancedSettingChanged() {
    emit advancedSettingChanged();
}

bool QnCameraSettingsWidget::isValidMotionRegion(){
    if (mode() == SingleMode)
        return m_singleWidget->isValidMotionRegion();
    return true;
}
