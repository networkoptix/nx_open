#include "camera_advanced_settings_xml_parser.h"
#include "ui/widgets/properties/camera_advanced_settings_widget.h"

const QString CASXP_IMAGING_GROUP_NAME(QLatin1String("%%Imaging"));
const QString CASXP_MAINTENANCE_GROUP_NAME(QLatin1String("%%Maintenance"));

//
// class CameraSettingsLister
//

CameraSettingsLister::CameraSettingsLister(const QString& filepath):
    CameraSettingReader(filepath, QString::fromLatin1("ONVIF"))
{

}

CameraSettingsLister::~CameraSettingsLister()
{

}

QStringList CameraSettingsLister::fetchParams()
{
    read();
    proceed();

    return m_params;
}

bool CameraSettingsLister::isGroupEnabled(const QString& id, const QString& /*parentId*/, const QString& /*name*/)
{
    m_params.push_back(id);

    return true;
}

bool CameraSettingsLister::isParamEnabled(const QString& id, const QString& /*parentId*/)
{
    m_params.push_back(id);

    //We don't need creation of the object, so returning false
    return false;
}

void CameraSettingsLister::paramFound(const CameraSetting& /*value*/, const QString& /*parentId*/)
{
    //This code is unreachable
    Q_ASSERT(false);
}

void CameraSettingsLister::cleanDataOnFail()
{
    m_params.clear();
}

//
// class CameraSettingsWidgetsCreator
//

CameraSettingsWidgetsCreator::CameraSettingsWidgetsCreator(const QString& filepath, QTabWidget& rootWidget, QObject* handler):
    CameraSettingReader(filepath, QString::fromLatin1("ONVIF")),
    m_settings(0),
    m_rootWidget(rootWidget),
    m_handler(handler)
{
    
}

CameraSettingsWidgetsCreator::~CameraSettingsWidgetsCreator()
{

}

bool CameraSettingsWidgetsCreator::recreateWidgets(CameraSettings* settings)
{
    if (!settings) {
        return false;
    }

    m_settings = settings;

    IndexById::Iterator it = m_indexById.begin();
    for (; it != m_indexById.end(); ++it)
    {
        m_rootWidget.removeTab(it.value());

        WidgetsById::Iterator wIt = m_widgetsById.find(it.key());
        if (wIt == m_widgetsById.end()) {
            //Object must be in hash
            Q_ASSERT(false);
        }

        delete wIt.value();
    }

    m_indexById.clear();
    m_widgetsById.clear();

    return read() && proceed();
}

bool CameraSettingsWidgetsCreator::isGroupEnabled(const QString& id, const QString& parentId, const QString& name)
{
    if (m_settings->find(id) != m_settings->end()) {
        //By default, group is enabled. It can be disabled by mediaserver
        //Disabled - means empty value. Group can have only empty value, so 
        //if we found it - it is disabled
        return false;
    }

    QWidget* tabWidget = 0;

    if (parentId.isEmpty())
    {
        tabWidget = new QWidget();
        m_indexById.insert(id, m_rootWidget.addTab(tabWidget, QString())); // TODO:        vvvv   strange code
        m_rootWidget.setTabText(m_rootWidget.indexOf(tabWidget), QApplication::translate("SingleCameraSettingsWidget",
            name.toLatin1().data(), 0, QApplication::UnicodeUTF8));

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

        QWidget* tabWidget = new QGroupBox(name);
        layout->addWidget(tabWidget);

        //tabWidget->setFixedWidth(300);
        //tabWidget->setFixedHeight(300);
    }

    tabWidget->setObjectName(id);
    m_widgetsById.insert(id, tabWidget);

    return true;
}

bool CameraSettingsWidgetsCreator::isParamEnabled(const QString& id, const QString& /*parentId*/)
{
    CameraSettings::ConstIterator it = m_settings->find(id);
    if (it == m_settings->end()) {
        //It means enabled, but have no value
        return true;
    }

    return isEnabled(*(it.value()));
}

void CameraSettingsWidgetsCreator::paramFound(const CameraSetting& value, const QString& parentId)
{
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

    CameraSettings::Iterator currIt = m_settings->find(value.getId());

    QWidget* tabWidget = 0;
    switch(value.getType())
    {
        case CameraSetting::OnOff:
            tabWidget = new QnSettingsOnOffWidget(m_handler, *(currIt.value()), *(parentIt.value()));
            break;

        case CameraSetting::MinMaxStep:
            tabWidget = new QnSettingsMinMaxStepWidget(m_handler, *(currIt.value()), *(parentIt.value()));
            break;

        case CameraSetting::Enumeration:
            tabWidget = new QnSettingsEnumerationWidget(m_handler, *(currIt.value()), *(parentIt.value()));
            break;

        case CameraSetting::Button:
            tabWidget = new QnSettingsButtonWidget(m_handler, *(parentIt.value()));
            break;

        default:
            //Unknown widget type!
            Q_ASSERT(false);
    }
    //tabWidget->move(0, 50 * currNum);

    m_widgetsById.insert(value.getId(), tabWidget);
}

void CameraSettingsWidgetsCreator::cleanDataOnFail()
{
    m_widgetsById.clear();
}
