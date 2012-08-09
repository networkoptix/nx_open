#include "single_camera_settings_widget.h"
#include "ui_single_camera_settings_widget.h"

#include <QtCore/QUrl>
#include <QtGui/QMessageBox>
#include <QtGui/QDesktopServices>

#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>
#include <core/resourcemanagment/resource_pool.h>

#include <ui/common/read_only.h>
#include <ui/widgets/properties/camera_schedule_widget.h>
#include <ui/widgets/properties/camera_motion_mask_widget.h>
#include <ui/graphics/items/resource/resource_widget.h>

QnSingleCameraSettingsWidget::QnSingleCameraSettingsWidget(QWidget *parent):
    QWidget(parent),
    ui(new Ui::SingleCameraSettingsWidget),
    m_motionWidget(NULL),
    m_hasCameraChanges(false),
    m_hasDbChanges(false),
    m_hasScheduleChanges(false),
    m_readOnly(false),
    m_inUpdateMaxFps(false),
    m_cameraSupportsMotion(false)
{
    ui->setupUi(this);

    m_motionLayout = new QVBoxLayout(ui->motionWidget);
    m_motionLayout->setContentsMargins(0, 0, 0, 0);
    
    connect(ui->tabWidget,              SIGNAL(currentChanged(int)),            this,   SLOT(at_tabWidget_currentChanged()));
    at_tabWidget_currentChanged();

    connect(ui->nameEdit,               SIGNAL(textChanged(const QString &)),   this,   SLOT(at_dataChanged()));
    connect(ui->checkBoxEnableAudio,    SIGNAL(stateChanged(int)),              this,   SLOT(at_dataChanged()));
    connect(ui->loginEdit,              SIGNAL(textChanged(const QString &)),   this,   SLOT(at_dataChanged()));
    connect(ui->passwordEdit,           SIGNAL(textChanged(const QString &)),   this,   SLOT(at_dataChanged()));
    connect(ui->cameraScheduleWidget,   SIGNAL(scheduleTasksChanged()),         this,   SLOT(at_cameraScheduleWidget_scheduleTasksChanged()));
    connect(ui->cameraScheduleWidget,   SIGNAL(gridParamsChanged()),            this,   SLOT(updateMaxFPS()));
    connect(ui->cameraScheduleWidget,   SIGNAL(scheduleEnabledChanged()),       this,   SLOT(at_dataChanged()));
    connect(ui->cameraScheduleWidget,   SIGNAL(moreLicensesRequested()),        this,   SIGNAL(moreLicensesRequested()));
    connect(ui->webPageLabel,           SIGNAL(linkActivated(const QString &)), this,   SLOT(at_linkActivated(const QString &)));
    connect(ui->motionWebPageLabel,     SIGNAL(linkActivated(const QString &)), this,   SLOT(at_linkActivated(const QString &)));

    connect(ui->cameraMotionButton,     SIGNAL(clicked(bool)),                  this,   SLOT(at_motionTypeChanged()));
    connect(ui->softwareMotionButton,   SIGNAL(clicked(bool)),                  this,   SLOT(at_motionTypeChanged()));
    connect(ui->sensitivitySlider,      SIGNAL(valueChanged(int)),              this,   SLOT(updateMotionWidgetSensitivity()));
    connect(ui->resetMotionRegionsButton, SIGNAL(clicked()),                    this,   SLOT(at_motionSelectionCleared()));

    connect(ui->advancedCheckBox1,      SIGNAL(stateChanged(int)),              this,   SLOT(updateAdvancedCheckboxValue()));

    loadSettingsFromXml();

    updateFromResource();
}

QnSingleCameraSettingsWidget::~QnSingleCameraSettingsWidget() {
    return;
}

void QnSingleCameraSettingsWidget::loadSettingsFromXml()
{
    QString error;
    //if (loadSettingsFromXml(QCoreApplication::applicationDirPath() + QLatin1String("/plugins/resources/camera_settings/CameraSettings.xml"), error))
    if (loadSettingsFromXml(QLatin1String("C:\\projects\\networkoptix\\netoptix_vms33\\common\\resource\\plugins\\resources\\camera_settings\\CameraSettings.xml"), error))
    {
        CL_LOG(cl_logINFO)
        {
            QString msg;
            QTextStream str(&msg);
            str << QLatin1String("XML loaded");
            cl_log.log(msg, cl_logINFO);
        }
    }
    else
    {
        CL_LOG(cl_logERROR)
        {
            QString log = QLatin1String("Cannot load devices list. Error:");
            log += error;
            cl_log.log(log, cl_logERROR);
        }

        cl_log.log(QLatin1String("Cannot load arecontvision/devices.xml"), cl_logERROR);
        /*QMessageBox msgBox;
        msgBox.setText(QMessageBox::tr("Error"));
        msgBox.setInformativeText(QMessageBox::tr("Cannot load /arecontvision/devices.xml"));
        msgBox.setStandardButtons(QMessageBox::Ok);
        msgBox.setDefaultButton(QMessageBox::Ok);
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.exec();*/
    }
}

bool QnSingleCameraSettingsWidget::loadSettingsFromXml(const QString& filepath, QString& error)
{
    QFile file(filepath);

    if (!file.exists())
    {
        error = QLatin1String("Cannot open file ");
        error += filepath;
        return false;
    }

    QString errorStr;
    int errorLine;
    int errorColumn;
    QDomDocument doc;

    if (!doc.setContent(&file, &errorStr, &errorLine, &errorColumn))
    {
        QTextStream str(&error);
        str << QLatin1String("File ") << filepath << QLatin1String("; Parse error at line ") << errorLine << QLatin1String(" column ") << errorColumn << QLatin1String(" : ") << errorStr;
        return false;
    }

    QDomElement root = doc.documentElement();
    if (root.tagName() != QLatin1String("cameras"))
        return false;

    QDomNode node = root.firstChild();

    while (!node.isNull())
    {
        if (node.toElement().tagName() == QLatin1String("camera"))
        {
            if (node.attributes().namedItem(QLatin1String("name")).nodeValue() != QLatin1String("ONVIF")) {
                continue;
            }
            return parseCameraXml(node.toElement(), error);
        }

        node = node.nextSibling();
    }

    return true;
}

bool QnSingleCameraSettingsWidget::parseGroupXml(const QDomElement &groupXml, const QString parentId, QString& error)
{
    if (groupXml.isNull()) {
        return true;
    }

    QString tagName = groupXml.nodeName();

    if (tagName == QLatin1String("param") && !parseElementXml(groupXml, parentId, error)) {
        return false;
    }

    if (tagName == QLatin1String("group"))
    {
        QString name = groupXml.attribute(QLatin1String("name"));
        if (name.isEmpty()) {
            error += QLatin1String("One of the tags '");
            error += tagName;
            error += QLatin1String("' doesn't have attribute 'name'");
            return false;
        }
        QString id = parentId + QLatin1String("%%") + name;

        if (m_widgetsById.find(id) != m_widgetsById.end()) {
            //Id must be unique!
            Q_ASSERT(false);
        }

        QWidgetPtr tabWidget(0);
        if (parentId.isEmpty()) {
            tabWidget = QWidgetPtr(new QWidget());
            ui->tabWidget->addTab(tabWidget.data(), QString()); // TODO:        vvvv   strange code
            ui->tabWidget->setTabText(ui->tabWidget->indexOf(tabWidget.data()), QApplication::translate("SingleCameraSettingsWidget", name.toStdString().c_str(), 0, QApplication::UnicodeUTF8));
        } else {
            WidgetsById::ConstIterator it = m_widgetsById.find(parentId);
            if (it == m_widgetsById.end()) {
                //Parent must be already created!
                Q_ASSERT(false);
            }
            QVBoxLayout *layout = dynamic_cast<QVBoxLayout *>((*it)->layout());
            if(!layout) {
                delete (*it)->layout();
                (*it)->setLayout(layout = new QVBoxLayout());
            }

            QGroupBox* groupBox = new QGroupBox(name);
            layout->addWidget(groupBox);

            tabWidget = QWidgetPtr(groupBox);
            groupBox->setObjectName(id);
            //groupBox->setFixedWidth(300);
            //groupBox->setFixedHeight(300);
        }
        tabWidget->setObjectName(name);
        m_widgetsById.insert(id, tabWidget);

        if (!parseGroupXml(groupXml.firstChildElement(), id, error)) {
            return false;
        }
    }

    if (!parseGroupXml(groupXml.nextSiblingElement(), parentId, error)) {
        return false;
    }

    return true;
}

bool QnSingleCameraSettingsWidget::parseElementXml(const QDomElement &elementXml, const QString parentId, QString& error)
{
    if (elementXml.isNull() || elementXml.nodeName() != QLatin1String("param")) {
        return true;
    }

    QString name = elementXml.attribute(QLatin1String("name"));
    if (name.isEmpty()) {
        error += QLatin1String("One of the tags 'param' doesn't have attribute 'name'");
        return false;
    }

    QString id = parentId + QLatin1String("%%") + name;

    QString widgetTypeStr = elementXml.attribute(QLatin1String("widget_type"));
    if (widgetTypeStr.isEmpty()) {
        error += QLatin1String("One of the tags 'param' doesn't have attribute 'widget_type'");
        return false;
    }
    CameraSetting::WIDGET_TYPE widgetType = CameraSetting::typeFromStr(widgetTypeStr);
    if (widgetType == CameraSetting::None) {
        error += QLatin1String("One of the tags 'param' has unexpected value of attribute 'widget_type': ") + widgetTypeStr;
        return false;
    }

    CameraSettingValue min = CameraSettingValue(elementXml.attribute(QLatin1String("min")));
    CameraSettingValue max = CameraSettingValue(elementXml.attribute(QLatin1String("max")));
    CameraSettingValue step = CameraSettingValue(elementXml.attribute(QLatin1String("step")));
    CameraSettingValue current = CameraSettingValue();
    //ToDo: remove following lines:
    if (widgetType != CameraSetting::Enumeration)
    {
        min = CameraSettingValue(QLatin1String("0"));
        max = CameraSettingValue(QLatin1String("100"));
        step = CameraSettingValue(QLatin1String("5"));
        current = CameraSettingValue(QLatin1String("20"));
    }

    WidgetsById::ConstIterator parentIt = m_widgetsById.find(parentId);
    if (parentIt == m_widgetsById.end()) {
        //Parent must be already created!
        Q_ASSERT(false);
    }

    QVBoxLayout *layout = dynamic_cast<QVBoxLayout *>((*parentIt)->layout());
    if(!layout) {
        delete (*parentIt)->layout();
        (*parentIt)->setLayout(layout = new QVBoxLayout());
    }
    //int currNum = (*parentIt)->children().length();

    if (m_widgetsById.find(id) != m_widgetsById.end()) {
        //Id must be unique!
        Q_ASSERT(false);
    }
    CameraSettings::Iterator currIt = m_cameraSettings.insert(id, CameraSetting(id, name, widgetType, min, max, step, current));

    QWidgetPtr tabWidget = QWidgetPtr(0);
    switch(widgetType)
    {
        case CameraSetting::OnOff:
            tabWidget = QWidgetPtr(new QnSettingsOnOffWidget(this, currIt.value(), *(parentIt.value().data())));
            break;

        case CameraSetting::MinMaxStep:
            tabWidget = QWidgetPtr(new QnSettingsMinMaxStepWidget(this, currIt.value(), *(parentIt.value().data())));
            break;

        case CameraSetting::Enumeration:
            tabWidget = QWidgetPtr(new QnSettingsEnumerationWidget(this, currIt.value(), *(parentIt.value().data())));
            break;

        case CameraSetting::Button:
            tabWidget = QWidgetPtr(new QnSettingsButtonWidget(this, currIt.value(), *(parentIt.value().data())));
            break;
        default:
            //Unknown widget type!
            Q_ASSERT(false);
    }
    //tabWidget->move(0, 50 * currNum);

    m_widgetsById.insert(id, QWidgetPtr(tabWidget));

    return true;
}

bool QnSingleCameraSettingsWidget::parseCameraXml(const QDomElement &cameraXml, QString& error)
{
    if (!parseGroupXml(cameraXml.firstChildElement(), QLatin1String(""), error)) {
        m_cameraSettings.clear();
        m_widgetsById.clear();
        return false;
    }

    return true;
}

const QnVirtualCameraResourcePtr &QnSingleCameraSettingsWidget::camera() const {
    return m_camera;
}

void QnSingleCameraSettingsWidget::setCamera(const QnVirtualCameraResourcePtr &camera) {
    if(m_camera == camera)
        return;

    m_camera = camera;
    
    updateFromResource();
}

Qn::CameraSettingsTab QnSingleCameraSettingsWidget::currentTab() const {
    /* Using field names here so that changes in UI file will lead to compilation errors. */

    QWidget *tab = ui->tabWidget->currentWidget();
    
    if(tab == ui->tabGeneral) {
        return Qn::GeneralSettingsTab;
    } else if(tab == ui->tabNetwork) {
        return Qn::NetworkSettingsTab;
    } else if(tab == ui->tabRecording) {
        return Qn::RecordingSettingsTab;
    } else if(tab == ui->tabMotion) {
        return Qn::MotionSettingsTab;
    } else if(tab == ui->tabAdvanced) {
        return Qn::AdvancedSettingsTab;
    } else {
        qnWarning("Current tab with index %1 was not recognized.", ui->tabWidget->currentIndex());
        return Qn::GeneralSettingsTab;
    }
}

void QnSingleCameraSettingsWidget::setCurrentTab(Qn::CameraSettingsTab tab) {
    /* Using field names here so that changes in UI file will lead to compilation errors. */

    switch(tab) {
    case Qn::GeneralSettingsTab:
        ui->tabWidget->setCurrentWidget(ui->tabGeneral);
        break;
    case Qn::NetworkSettingsTab:
        ui->tabWidget->setCurrentWidget(ui->tabNetwork);
        break;
    case Qn::RecordingSettingsTab:
        ui->tabWidget->setCurrentWidget(ui->tabRecording);
        break;
    case Qn::MotionSettingsTab:
        ui->tabWidget->setCurrentWidget(ui->tabMotion);
        break;
    case Qn::AdvancedSettingsTab:
        ui->tabWidget->setCurrentWidget(ui->tabAdvanced);
        break;
    default:
        qnWarning("Invalid camera settings tab '%1'.", static_cast<int>(tab));
        break;
    }
}

bool QnSingleCameraSettingsWidget::isCameraActive() const {
    return ui->cameraScheduleWidget->activeCameraCount() != 0;
}

void QnSingleCameraSettingsWidget::setCameraActive(bool active) {
    ui->cameraScheduleWidget->setScheduleEnabled(active);
}

void QnSingleCameraSettingsWidget::submitToResource() {
    if(!m_camera)
        return;

    if (hasDbChanges()) {
        m_camera->setName(ui->nameEdit->text());
        m_camera->setAudioEnabled(ui->checkBoxEnableAudio->isChecked());
        m_camera->setUrl(ui->ipAddressEdit->text());
        m_camera->setAuth(ui->loginEdit->text(), ui->passwordEdit->text());
        m_camera->setScheduleDisabled(ui->cameraScheduleWidget->activeCameraCount() == 0);

        if (m_hasScheduleChanges) {
            QnScheduleTaskList scheduleTasks;
            foreach (const QnScheduleTask::Data& scheduleTaskData, ui->cameraScheduleWidget->scheduleTasks())
                scheduleTasks.append(QnScheduleTask(scheduleTaskData));
            m_camera->setScheduleTasks(scheduleTasks);
        }

        if (ui->cameraMotionButton->isChecked())
            m_camera->setMotionType(m_camera->getCameraBasedMotionType());
        else
            m_camera->setMotionType(MT_SoftwareGrid);

        m_camera->setAdvancedWorking(ui->advancedCheckBox1->isChecked());

        submitMotionWidgetToResource();

        setHasDbChanges(false);
    }

    if (hasCameraChanges()) {
        m_camera->setAdvancedWorking(ui->advancedCheckBox1->isChecked());

        setHasCameraChanges(false);
    }
}

void QnSingleCameraSettingsWidget::updateFromResource() {
    if(!m_camera) {
        ui->nameEdit->setText(QString());
        ui->checkBoxEnableAudio->setChecked(false);
        ui->macAddressEdit->setText(QString());
        ui->ipAddressEdit->setText(QString());
        ui->webPageLabel->setText(QString());
        ui->loginEdit->setText(QString());
        ui->passwordEdit->setText(QString());

        ui->cameraScheduleWidget->setScheduleTasks(QnScheduleTaskList());
        ui->cameraScheduleWidget->setScheduleEnabled(false);
        ui->cameraScheduleWidget->setChangesDisabled(false); /* Do not block editing by default if schedule task list is empty. */
        ui->cameraScheduleWidget->setCameras(QnVirtualCameraResourceList());

        ui->softwareMotionButton->setEnabled(false);
        ui->cameraMotionButton->setText(QString());
        ui->motionWebPageLabel->setText(QString());
        ui->cameraMotionButton->setChecked(false);
        ui->softwareMotionButton->setChecked(false);
        
        m_cameraSupportsMotion = false;
        ui->motionSettingsGroupBox->setEnabled(false);
        ui->motionAvailableLabel->setVisible(true);

        ui->advancedCheckBox1->setChecked(false);
    } else {
        QString webPageAddress = QString(QLatin1String("http://%1")).arg(m_camera->getHostAddress().toString());

        ui->nameEdit->setText(m_camera->getName());
        ui->checkBoxEnableAudio->setChecked(m_camera->isAudioEnabled());
        ui->checkBoxEnableAudio->setEnabled(m_camera->isAudioSupported());
        ui->macAddressEdit->setText(m_camera->getMAC().toString());
        ui->ipAddressEdit->setText(m_camera->getUrl());
        ui->webPageLabel->setText(tr("<a href=\"%1\">%2</a>").arg(webPageAddress).arg(webPageAddress));
        ui->loginEdit->setText(m_camera->getAuth().user());
        ui->passwordEdit->setText(m_camera->getAuth().password());

        ui->softwareMotionButton->setEnabled(m_camera->supportedMotionType() & MT_SoftwareGrid);
        if (m_camera->supportedMotionType() & (MT_HardwareGrid | MT_MotionWindow))
            ui->cameraMotionButton->setText(tr("Hardware (Camera built-in)"));
        else
            ui->cameraMotionButton->setText(tr("Do not record motion"));

        QnVirtualCameraResourceList cameras;
        cameras.push_back(m_camera);
        ui->cameraScheduleWidget->setCameras(cameras);

        QList<QnScheduleTask::Data> scheduleTasks;
        foreach (const QnScheduleTask& scheduleTaskData, m_camera->getScheduleTasks())
            scheduleTasks.append(scheduleTaskData.getData());
        ui->cameraScheduleWidget->setScheduleTasks(scheduleTasks);
        
        ui->cameraScheduleWidget->setScheduleEnabled(!m_camera->isScheduleDisabled());

        int currentCameraFps = ui->cameraScheduleWidget->getGridMaxFps();
        if (currentCameraFps > 0)
            ui->cameraScheduleWidget->setFps(currentCameraFps);
        else
            ui->cameraScheduleWidget->setFps(ui->cameraScheduleWidget->getMaxFps()/2);

        updateMaxFPS();

        ui->motionWebPageLabel->setText(webPageAddress); // TODO: wrong, need to get camera-specific web page
        ui->cameraMotionButton->setChecked(m_camera->getMotionType() != MT_SoftwareGrid);
        ui->softwareMotionButton->setChecked(m_camera->getMotionType() == MT_SoftwareGrid);

        m_cameraSupportsMotion = m_camera->supportedMotionType() != MT_NoMotion;
        ui->motionSettingsGroupBox->setEnabled(m_cameraSupportsMotion);
        ui->motionAvailableLabel->setVisible(!m_cameraSupportsMotion);

        ui->advancedCheckBox1->setChecked(m_camera->isAdvancedWorking());
    }

    updateMotionWidgetFromResource();
    updateMotionAvailability();

    setHasDbChanges(false);
    setHasCameraChanges(false);
}

void QnSingleCameraSettingsWidget::updateMotionWidgetFromResource() {
    if(!m_motionWidget)
        return;

    disconnectFromMotionWidget();

    m_motionWidget->setCamera(m_camera);
    if(!m_camera) {
        m_motionWidget->setVisible(false);
    } else {
        m_motionWidget->setVisible(m_cameraSupportsMotion);
    }
    updateMotionWidgetNeedControlMaxRect();
    ui->sensitivitySlider->setValue(m_motionWidget->motionSensitivity());

    connectToMotionWidget();
}

void QnSingleCameraSettingsWidget::submitMotionWidgetToResource() {
    if(!m_motionWidget || !m_camera)
        return;

    m_camera->setMotionRegionList(m_motionWidget->motionRegionList(), QnDomainMemory);
}

bool QnSingleCameraSettingsWidget::isReadOnly() const {
    return m_readOnly;
}

void QnSingleCameraSettingsWidget::setReadOnly(bool readOnly) {
    if(m_readOnly == readOnly)
        return;

    using ::setReadOnly;
    setReadOnly(ui->nameEdit, readOnly);
    setReadOnly(ui->checkBoxEnableAudio, readOnly);
    setReadOnly(ui->loginEdit, readOnly);
    setReadOnly(ui->passwordEdit, readOnly);
    setReadOnly(ui->cameraScheduleWidget, readOnly);
    if(m_motionWidget)
        setReadOnly(m_motionWidget, readOnly);
    m_readOnly = readOnly;
}

void QnSingleCameraSettingsWidget::setHasDbChanges(bool hasChanges) {
    if(m_hasDbChanges == hasChanges)
        return;

    m_hasDbChanges = hasChanges;
    if(!m_hasDbChanges && !hasCameraChanges())
        m_hasScheduleChanges = false;

    emit hasChangesChanged();
}

void QnSingleCameraSettingsWidget::setHasCameraChanges(bool hasChanges) {
    if(m_hasCameraChanges == hasChanges)
        return;

    m_hasCameraChanges = hasChanges;
    if(!m_hasCameraChanges && !hasDbChanges())
        m_hasScheduleChanges = false;

    emit hasChangesChanged();
}

void QnSingleCameraSettingsWidget::disconnectFromMotionWidget() {
    assert(m_motionWidget);

    disconnect(m_motionWidget, NULL, this, NULL);
}

void QnSingleCameraSettingsWidget::connectToMotionWidget() {
    assert(m_motionWidget);

    connect(m_motionWidget, SIGNAL(motionRegionListChanged()), this, SLOT(at_dataChanged()), Qt::UniqueConnection);
}

void QnSingleCameraSettingsWidget::updateMotionWidgetNeedControlMaxRect() {
    if(!m_motionWidget)
        return;
    bool hwMotion = m_camera && (m_camera->supportedMotionType() & (MT_HardwareGrid | MT_MotionWindow));
    m_motionWidget->setNeedControlMaxRects(m_cameraSupportsMotion && hwMotion && !ui->softwareMotionButton->isChecked());
}

void QnSingleCameraSettingsWidget::updateMotionAvailability() {
    bool motionAvailable = true;
    if(ui->cameraMotionButton->isChecked())
        motionAvailable = m_camera && (m_camera->getCameraBasedMotionType() != MT_NoMotion);

    ui->cameraScheduleWidget->setMotionAvailable(motionAvailable);
}

void QnSingleCameraSettingsWidget::updateMotionWidgetSensitivity() {
    if(m_motionWidget)
        m_motionWidget->setMotionSensitivity(ui->sensitivitySlider->value());
}

bool QnSingleCameraSettingsWidget::isValidMotionRegion(){
    if (!m_motionWidget) 
        return true;
    return m_motionWidget->isValidMotionRegion();
}

void QnSingleCameraSettingsWidget::updateAdvancedCheckboxValue() {
    bool result = ui->advancedCheckBox1->isChecked();
    at_cameraDataChanged();
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnSingleCameraSettingsWidget::hideEvent(QHideEvent *event) {
    base_type::hideEvent(event);

    if(m_motionWidget) {
        disconnectFromMotionWidget();
        m_motionWidget->setCamera(QnResourcePtr());
    }
}

void QnSingleCameraSettingsWidget::showEvent(QShowEvent *event) {
    base_type::showEvent(event);
    
    if(m_motionWidget) {
        updateMotionWidgetFromResource();
        connectToMotionWidget();
    }

    at_tabWidget_currentChanged(); /* Re-create motion widget if needed. */

    updateFromResource();
}

void QnSingleCameraSettingsWidget::at_motionTypeChanged() {
    at_dbDataChanged();

    updateMotionWidgetNeedControlMaxRect();
    updateMaxFPS();
    updateMotionAvailability();
}

void QnSingleCameraSettingsWidget::updateMaxFPS() {
    if (m_inUpdateMaxFps)
        return; /* Do not show message twice. */

    m_inUpdateMaxFps = true;
    if ((ui->softwareMotionButton->isEnabled() &&  ui->softwareMotionButton->isChecked())
        || ui->cameraScheduleWidget->isSecondaryStreamReserver()) {
        float maxFps = m_camera->getMaxFps()-2;
        float currentMaxFps = ui->cameraScheduleWidget->getGridMaxFps();
        if (currentMaxFps > maxFps)
        {
            QMessageBox::warning(this, tr("Warning. FPS value is too high"), 
                tr("For software motion 2 fps is reserved for secondary stream. Current fps in schedule grid is %1. Fps dropped down to %2").arg(currentMaxFps).arg(maxFps));
        }
        ui->cameraScheduleWidget->setMaxFps(maxFps);
    } else {
        ui->cameraScheduleWidget->setMaxFps(m_camera->getMaxFps());
    }
    m_inUpdateMaxFps = false;
}

void QnSingleCameraSettingsWidget::at_motionSelectionCleared() {
    if (m_motionWidget)
        m_motionWidget->clearMotion();
}

void QnSingleCameraSettingsWidget::at_linkActivated(const QString &urlString) {
    QUrl url(urlString);

    if (!m_readOnly) {
        url.setUserName(ui->loginEdit->text());
        url.setPassword(ui->passwordEdit->text());
    }

    QDesktopServices::openUrl(url);
}

void QnSingleCameraSettingsWidget::at_tabWidget_currentChanged() {
    if(m_motionWidget != NULL)
        return;

    if(currentTab() != Qn::MotionSettingsTab)
        return;

    m_motionWidget = new QnCameraMotionMaskWidget(this);
    updateMotionWidgetFromResource();
    
    using ::setReadOnly;
    setReadOnly(m_motionWidget, m_readOnly);
    
    m_motionLayout->addWidget(m_motionWidget);

    connectToMotionWidget();
}

void QnSingleCameraSettingsWidget::at_dbDataChanged() {
    setHasDbChanges(true);
}

void QnSingleCameraSettingsWidget::at_cameraDataChanged() {
    setHasCameraChanges(true);
}

void QnSingleCameraSettingsWidget::at_cameraScheduleWidget_scheduleTasksChanged() {
    at_dbDataChanged();
    at_cameraDataChanged();

    m_hasScheduleChanges = true;
}
