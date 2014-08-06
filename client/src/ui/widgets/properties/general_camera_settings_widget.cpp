#include "general_camera_settings_widget.h"
#include "ui_general_camera_settings_widget.h"

#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>

#include <ui/actions/action_manager.h>
#include <ui/common/read_only.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/workaround/qt5_combobox_workaround.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_access_controller.h>

#include <utils/common/scoped_value_rollback.h>

namespace {

    template<typename T>
    bool isSameValue(const QnVirtualCameraResourceList &cameras, std::function<T(const QnVirtualCameraResourcePtr &camera)> member, T& firstValue) {
        bool isFirst = true;
        foreach (const QnVirtualCameraResourcePtr &camera, cameras) {
            if (isFirst) {
                firstValue = member(camera);
                isFirst = false;
                continue;
            }

            if (member(camera) != firstValue)
                return false;
        }
        return true;
    }


    void updateCheckbox(QCheckBox* checkbox, const QnVirtualCameraResourceList &cameras, std::function<bool(const QnVirtualCameraResourcePtr &camera)> member) {
        bool value;
        if (isSameValue<bool>(cameras, member, value)) {
            checkbox->setTristate(false);
            checkbox->setChecked(value);
        } else {
            checkbox->setTristate(true);
            checkbox->setCheckState(Qt::PartiallyChecked);
        }
    }


    const char *oldValuePropertyName = "_qn_oldValue";
}

QnGeneralCameraSettingsWidget::QnGeneralCameraSettingsWidget(QWidget* parent /*= 0*/):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    m_updating(false),
    m_readOnly(false)
{

    setHelpTopic(ui->fisheyeCheckBox,                                       Qn::CameraSettings_Dewarping_Help);
    setHelpTopic(ui->nameLabel,         ui->nameEdit,                       Qn::CameraSettings_General_Name_Help);
    setHelpTopic(ui->modelLabel,        ui->modelEdit,                      Qn::CameraSettings_General_Model_Help);
    setHelpTopic(ui->firmwareLabel,     ui->firmwareEdit,                   Qn::CameraSettings_General_Firmware_Help);
    //TODO: #Elric add context help for vendor field
    setHelpTopic(ui->addressGroupBox,                                       Qn::CameraSettings_General_Address_Help);
    setHelpTopic(ui->enableAudioCheckBox,                                   Qn::CameraSettings_General_Audio_Help);
    setHelpTopic(ui->authenticationGroupBox,                                Qn::CameraSettings_General_Auth_Help);
    setHelpTopic(ui->forceArCheckBox, ui->forceArComboBox,                  Qn::CameraSettings_AspectRatio_Help);

    connect(context(),                  &QnWorkbenchContext::userChanged,   this,   &QnGeneralCameraSettingsWidget::updateLicensesButtonVisible);

    connect(ui->nameEdit,               &QLineEdit::textChanged,            this,   &QnGeneralCameraSettingsWidget::at_dataChanged);
    connect(ui->enableAudioCheckBox,    &QCheckBox::stateChanged,           this,   &QnGeneralCameraSettingsWidget::at_dataChanged);
    connect(ui->analogViewCheckBox,     &QCheckBox::stateChanged,           this,   &QnGeneralCameraSettingsWidget::at_dataChanged);
    connect(ui->fisheyeCheckBox,        &QCheckBox::stateChanged,           this,   &QnGeneralCameraSettingsWidget::at_dataChanged);
    connect(ui->loginEdit,              &QLineEdit::textChanged,            this,   &QnGeneralCameraSettingsWidget::at_dataChanged);
    connect(ui->passwordEdit,           &QLineEdit::textChanged,            this,   &QnGeneralCameraSettingsWidget::at_dataChanged);
    connect(ui->webPageLabel,           &QLabel::linkActivated,             this,   &QnGeneralCameraSettingsWidget::at_linkActivated);

    connect(ui->pingButton,             SIGNAL(clicked()),                  this,   SLOT(at_pingButton_clicked()));
    connect(ui->moreLicensesButton,     &QPushButton::clicked,              this,   [this]{menu()->trigger(Qn::PreferencesLicensesTabAction);});

    connect(ui->analogViewCheckBox,     SIGNAL(stateChanged(int)),          this,   SLOT(updateLicenseText()), Qt::QueuedConnection);
    connect(qnLicensePool,              SIGNAL(licensesChanged()),          this,   SLOT(updateLicenseText()), Qt::QueuedConnection);

    connect(ui->enableAudioCheckBox,    &QPushButton::clicked,              this,   &QnGeneralCameraSettingsWidget::at_enableAudioCheckBox_clicked);
    connect(ui->analogViewCheckBox,     &QPushButton::clicked,              this,   &QnGeneralCameraSettingsWidget::at_analogViewCheckBox_clicked);

    connect(ui->fisheyeCheckBox,        &QCheckBox::toggled,                this,   &QnGeneralCameraSettingsWidget::fisheyeSettingsChanged);

    connect(ui->forceArCheckBox,        &QCheckBox::stateChanged,           this,   [this](int state){ ui->forceArComboBox->setEnabled(state == Qt::Checked);} );
    connect(ui->forceArCheckBox,        &QCheckBox::stateChanged,           this,  &QnGeneralCameraSettingsWidget::at_dataChanged);

    ui->forceArComboBox->addItem(tr("4:3"),  4.0 / 3);
    ui->forceArComboBox->addItem(tr("16:9"), 16.0 / 9);
    ui->forceArComboBox->addItem(tr("1:1"),  1.0);
    ui->forceArComboBox->setCurrentIndex(0);
    connect(ui->forceArComboBox,        QnComboboxCurrentIndexChanged,      this,  &QnGeneralCameraSettingsWidget::at_dataChanged);

    connect(ui->forceRotationCheckBox,  &QCheckBox::stateChanged,           this,   [this](int state){ ui->forceRotationComboBox->setEnabled(state == Qt::Checked);} );
    connect(ui->forceRotationCheckBox,  &QCheckBox::stateChanged,           this,   &QnGeneralCameraSettingsWidget::at_dataChanged);

    ui->forceRotationComboBox->addItem(tr("0 degree"),      0);
    ui->forceRotationComboBox->addItem(tr("90 degrees"),    90);
    ui->forceRotationComboBox->addItem(tr("180 degrees"),   180);
    ui->forceRotationComboBox->addItem(tr("270 degrees"),   270);
    ui->forceRotationComboBox->setCurrentIndex(0);
    connect(ui->forceRotationComboBox,  QnComboboxCurrentIndexChanged,      this,   &QnGeneralCameraSettingsWidget::at_dataChanged);


    updateLicensesButtonVisible();
}

QnGeneralCameraSettingsWidget::~QnGeneralCameraSettingsWidget() {
    


}

QAuthenticator QnGeneralCameraSettingsWidget::authenticator() const {
    QAuthenticator result;
    result.setUser(ui->loginEdit->text());
    result.setPassword(ui->passwordEdit->text());
}


QnVirtualCameraResourceList QnGeneralCameraSettingsWidget::cameras() const {
    return m_cameras;
}

void QnGeneralCameraSettingsWidget::setCameras(const QnVirtualCameraResourceList &cameras) {
    if (m_cameras == cameras)
        return;

    if (m_cameras.size() == 1) {
        disconnect(m_cameras.first(), NULL, this, NULL);
    }

    m_cameras = cameras;

    if (m_cameras.size() == 1) {
        connect(m_cameras.first(), &QnResource::urlChanged,        this, &QnGeneralCameraSettingsWidget::updateIpAddressText);
        connect(m_cameras.first(), &QnResource::urlChanged,        this, &QnGeneralCameraSettingsWidget::updateWebPageText);
    }

}

bool QnGeneralCameraSettingsWidget::isReadOnly() const {
    return m_readOnly;
}

void QnGeneralCameraSettingsWidget::setReadOnly(bool readOnly) {
    if(m_readOnly == readOnly)
        return;

    using ::setReadOnly;
    setReadOnly(ui->nameEdit, readOnly);
    setReadOnly(ui->enableAudioCheckBox, readOnly);
    setReadOnly(ui->fisheyeCheckBox, readOnly);
    setReadOnly(ui->loginEdit, readOnly);
    setReadOnly(ui->passwordEdit, readOnly);

    m_readOnly = readOnly;
}


bool QnGeneralCameraSettingsWidget::isFisheyeEnabled() const {
    return ui->fisheyeCheckBox->isChecked();
}


void QnGeneralCameraSettingsWidget::updateFromResources() {
    if (m_cameras.isEmpty())
        return; //safety check

    QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);

    {  /* Setup audio. */
        bool audioSupported = true;
        if (!isSameValue<bool>(m_cameras, [](const QnVirtualCameraResourcePtr &camera){return camera->isAudioSupported();}, audioSupported)) 
            audioSupported = false;

        ui->enableAudioCheckBox->setEnabled(audioSupported);
        if (audioSupported) {
            updateCheckbox(ui->enableAudioCheckBox, m_cameras, [](const QnVirtualCameraResourcePtr &camera){return camera->isAudioEnabled();});
        } else {
            ui->enableAudioCheckBox->setTristate(true);
            ui->enableAudioCheckBox->setCheckState(Qt::PartiallyChecked);
        }
    }

    {  /* Setup analog licenses. */
        bool haveDtsBased = false;
        if (!isSameValue<bool>(m_cameras, [](const QnVirtualCameraResourcePtr &camera){return camera->isDtsBased();}, haveDtsBased)) 
            haveDtsBased = true;
        ui->analogGroupBox->setVisible(haveDtsBased);
        if (haveDtsBased)
            updateCheckbox(ui->analogViewCheckBox, m_cameras, [](const QnVirtualCameraResourcePtr &camera){return !camera->isScheduleDisabled();});
    }

    {  /* Setup forced aspect ratio. */
        QString forcedAr;
        if (isSameValue<QString>(m_cameras, [](const QnVirtualCameraResourcePtr &camera){return camera->getProperty(QnMediaResource::customAspectRatioKey());}, forcedAr)) {
            ui->forceArCheckBox->setTristate(false);
            ui->forceArCheckBox->setChecked(!forcedAr.isEmpty());
            if (!forcedAr.isEmpty()) {
                float ar = forcedAr.toFloat(); // float is important here
                int idx = -1;
                for (int i = 0; i < ui->forceArComboBox->count(); ++i) {
                    if (qFuzzyEquals(ar, ui->forceArComboBox->itemData(i).toFloat())) {
                        idx = i;
                        break;
                    }
                }
                ui->forceArComboBox->setCurrentIndex(idx < 0 ? 0 : idx);
            }
        } else {
            ui->forceArCheckBox->setTristate(true);
            ui->forceArCheckBox->setCheckState(Qt::PartiallyChecked);
            ui->forceArComboBox->setCurrentIndex(0);
        }
    }

    {  /* Setup forced rotation. */
        QString forcedRotation;
        if (isSameValue<QString>(m_cameras, [](const QnVirtualCameraResourcePtr &camera){return camera->getProperty(QnMediaResource::rotationKey());}, forcedRotation)) {
            ui->forceRotationCheckBox->setTristate(false);
            ui->forceRotationCheckBox->setChecked(!forcedRotation.isEmpty());
            if (!forcedRotation.isEmpty()) {
                int degree = forcedRotation.toInt();
                int idx = -1;
                for (int i = 0; i < ui->forceRotationComboBox->count(); ++i) {
                    if (degree == ui->forceRotationComboBox->itemData(i).toInt()) {
                        idx = i;
                        break;
                    }
                }
                ui->forceRotationComboBox->setCurrentIndex(idx < 0 ? 0 : idx);
            }
        } else {
            ui->forceRotationCheckBox->setTristate(true);
            ui->forceRotationCheckBox->setCheckState(Qt::PartiallyChecked);
            ui->forceRotationComboBox->setCurrentIndex(0);
        }
    }

    {  /* Setup authenticator. */
        QString login, password;
        if (isSameValue<QString>(m_cameras, [](const QnVirtualCameraResourcePtr &camera){ return camera->getAuth().user();}, login)) {
            ui->loginEdit->setText(login);
            ui->loginEdit->setPlaceholderText(QString());
        } else {
            ui->loginEdit->setText(QString());
            ui->loginEdit->setPlaceholderText(tr("<multiple values>", "LoginEdit"));
        }
        ui->loginEdit->setProperty(oldValuePropertyName, ui->loginEdit->text());

        if (isSameValue<QString>(m_cameras, [](const QnVirtualCameraResourcePtr &camera){ return camera->getAuth().password();}, password)) {
            ui->passwordEdit->setText(password);
            ui->passwordEdit->setPlaceholderText(QString());
        } else {
            ui->passwordEdit->setText(QString());
            ui->passwordEdit->setPlaceholderText(tr("<multiple values>", "PasswordEdit"));
        }
        ui->passwordEdit->setProperty(oldValuePropertyName, ui->passwordEdit->text());


    }

    bool isSingle = m_cameras.size() == 1;
    ui->pingButton->setVisible(isSingle);
    ui->descriptionWidget->setVisible(isSingle);
    ui->addressGroupBox->setVisible(isSingle);
    ui->fisheyeCheckBox->setVisible(isSingle);
    if (isSingle) {
        QnVirtualCameraResourcePtr firstCamera = m_cameras.first();

        ui->nameEdit->setText(firstCamera->getName());
        ui->modelEdit->setText(firstCamera->getModel());
        ui->firmwareEdit->setText(firstCamera->getFirmware());
        ui->vendorEdit->setText(firstCamera->getVendor());

        ui->macAddressEdit->setText(firstCamera->getMAC().toString());
        updateIpAddressText();
        updateWebPageText();
        
        /* There are fisheye cameras on the market that report themselves as PTZ.
         * We still want to be able to toggle them as fisheye instead, 
         * so this checkbox must always be enabled, even for PTZ cameras. */
        //ui->fisheyeCheckBox->setEnabled(true);
        ui->fisheyeCheckBox->setChecked(firstCamera->getDewarpingParams().enabled);
    }

}

void QnGeneralCameraSettingsWidget::submitToResources() {
    
    QString login = ui->loginEdit->text().trimmed();
    bool loginWasEmpty = ui->loginEdit->property(oldValuePropertyName).toString().isEmpty();
    QString password = ui->passwordEdit->text().trimmed();
    bool passwordWasEmpty = ui->passwordEdit->property(oldValuePropertyName).toString().isEmpty();
    bool overrideAr = ui->forceArCheckBox->checkState() == Qt::Checked;
    bool clearAr = ui->forceArCheckBox->checkState() == Qt::Unchecked;
    bool overrideRotation = ui->forceRotationCheckBox->checkState() == Qt::Checked;
    bool clearRotation = ui->forceRotationCheckBox->checkState() == Qt::Unchecked;

    foreach(QnVirtualCameraResourcePtr camera, m_cameras) 
    {
        QString cameraLogin = camera->getAuth().user();
        if (!login.isEmpty() || ! loginWasEmpty)
            cameraLogin = login;

        QString cameraPassword = camera->getAuth().password();
        if (!password.isEmpty() || !passwordWasEmpty)
            cameraPassword = password;

        camera->setAuth(cameraLogin, cameraPassword);

        if (ui->enableAudioCheckBox->checkState() != Qt::PartiallyChecked && ui->enableAudioCheckBox->isEnabled()) 
            camera->setAudioEnabled(ui->enableAudioCheckBox->isChecked());

        if (overrideAr)
            camera->setProperty(QnMediaResource::customAspectRatioKey(), QString::number(ui->forceArComboBox->currentData().toDouble()));
        else if (clearAr)
            camera->setProperty(QnMediaResource::customAspectRatioKey(), QString());

        if (overrideRotation) 
            camera->setProperty(QnMediaResource::rotationKey(), QString::number(ui->forceRotationComboBox->currentData().toInt()));
        else if (clearRotation)
            camera->setProperty(QnMediaResource::rotationKey(), QString());

    }

    if (m_cameras.size() == 1) {
        m_cameras.first()->setName(ui->nameEdit->text());
        if (m_cameras.first()->isDtsBased()) 
            m_cameras.first()->setScheduleDisabled(!ui->analogViewCheckBox->isChecked()); 
    }
}

void QnGeneralCameraSettingsWidget::updateLicensesButtonVisible() {
    ui->moreLicensesButton->setVisible(accessController()->globalPermissions() & Qn::GlobalProtectedPermission);
}


void QnGeneralCameraSettingsWidget::updateIpAddressText() {
    if (m_cameras.size() < 1)
        return;

    QString urlString = m_cameras.first()->getUrl();
    QUrl url = QUrl::fromUserInput(urlString);
    ui->ipAddressEdit->setText(!url.isEmpty() && url.isValid() ? url.host() : urlString);
}

void QnGeneralCameraSettingsWidget::updateWebPageText() {
    if (m_cameras.size() < 1)
        return;

    QString webPageAddress = QString(QLatin1String("http://%1")).arg(m_cameras.first()->getHostAddress());

    QUrl url = QUrl::fromUserInput(m_cameras.first()->getUrl());
    if(url.isValid()) {
        QUrlQuery query(url);
        int port = query.queryItemValue(lit("http_port")).toInt();
        if(port == 0)
            port = url.port(80);

        if (port != 80 && port > 0)
            webPageAddress += QLatin1Char(':') + QString::number(url.port());
    }

    ui->webPageLabel->setText(tr("<a href=\"%1\">%2</a>").arg(webPageAddress).arg(webPageAddress));

    if (!m_cameras.first()->isDtsBased()) {
        // TODO #Elric: wrong, need to get camera-specific web page
        ui->motionWebPageLabel->setText(tr("<a href=\"%1\">%2</a>").arg(webPageAddress).arg(webPageAddress));
    } else {
        ui->motionWebPageLabel->clear();
    }

}

void QnGeneralCameraSettingsWidget::at_dataChanged() {
    if (m_updating || m_cameras.isEmpty())
        return;
    emit dataChanged();
}


void QnGeneralCameraSettingsWidget::at_linkActivated(const QString &urlString) {
    QUrl url(urlString);
    QAuthenticator auth = authenticator();
    url.setUserName(auth.user());
    url.setPassword(auth.password());
    QDesktopServices::openUrl(url);
}



void QnGeneralCameraSettingsWidget::at_enableAudioCheckBox_clicked() {
    Qt::CheckState state = ui->enableAudioCheckBox->checkState();

    ui->enableAudioCheckBox->setTristate(false);
    if (state == Qt::PartiallyChecked)
        ui->enableAudioCheckBox->setCheckState(Qt::Checked);
}

void QnGeneralCameraSettingsWidget::at_analogViewCheckBox_clicked() {
    Qt::CheckState state = ui->analogViewCheckBox->checkState();

    ui->analogViewCheckBox->setTristate(false);
    if (state == Qt::PartiallyChecked)
        ui->analogViewCheckBox->setCheckState(Qt::Checked);
    ui->cameraScheduleWidget->setScheduleEnabled(ui->analogViewCheckBox->isChecked());
}

void QnGeneralCameraSettingsWidget::at_pingButton_clicked() {
    if (m_cameras.size() == 1)
        menu()->trigger(Qn::PingAction, QnActionParameters(m_cameras.first()));
    //do nothing otherwise
}
