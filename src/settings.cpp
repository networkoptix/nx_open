#include "settings.h"
#include "serial.h"

#include "base/log.h"

bool global_show_item_text = false;
qreal global_rotation_angel = 0;
//QFont settings_font("Bodoni MT", 12);
QFont settings_font;//("Bodoni MT", 12);

QColor global_shadow_color(0, 0, 0, 128);

QColor global_selection_color(0, 150, 255, 110);
QColor global_can_be_droped_color(0, 255, 150, 110);

// how often we run new device search and how often layout synchronizes with device manager 
int devices_update_interval = 1000;

QColor app_bkr_color(0,5,5,125);

qreal global_menu_opacity =  0.8;
qreal global_dlg_opacity  = 0.9;
qreal global_decoration_opacity  = 0.2;
qreal global_decoration_max_opacity  = 0.7;

qreal global_base_scene_z_level = 1.0;

int global_grid_aparence_delay = 2000;

int global_opacity_change_period = 1000;

Settings::Settings()
    : m_RWLock(QReadWriteLock::Recursive)
{
}

Settings& Settings::instance()
{
    static Settings settings;
    return settings;
}

void Settings::fillData(Settings::Data& data) const
{
    QReadLocker _lock(&m_RWLock);

    data = m_data;
}

void Settings::update(const Settings::Data& data)
{
    QWriteLocker _lock(&m_RWLock);

    setMediaRoot(data.mediaRoot);
    setAuxMediaRoots(data.auxMediaRoots);
    setAllowChangeIP(data.allowChangeIP);
}

void Settings::load(const QString& fileName)
{
    QWriteLocker _lock(&m_RWLock);

    m_fileName = fileName;

    reset();

    QFile settingsFile(m_fileName);

    if (!settingsFile.exists())
    {
        cl_log.log("Can't find settings file", cl_logERROR);
        return;
    }

    if (!settingsFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        cl_log.log("Can't open settings file", cl_logERROR);
        return;
    }

    QXmlStreamReader xml(&settingsFile);

    while (!xml.atEnd())
    {
        xml.readNext();

        if (xml.isStartElement())
        {
            if (xml.name() == "mediaRoot")
                m_data.mediaRoot = xml.readElementText();
            else if (xml.name() == "auxMediaRoot")
                m_data.auxMediaRoots.push_back(xml.readElementText());
            else if (xml.name() == "serialNumber")
                setSerialNumber(xml.readElementText());
        }
    }

    if (xml.hasError())
    {
        reset();
    }
}

void Settings::save()
{
    QWriteLocker _lock(&m_RWLock);

    QFile settingsFile(m_fileName);

    if (!settingsFile.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        cl_log.log("Can't open settings file", cl_logERROR);
        return;
    }

    QXmlStreamWriter stream(&settingsFile);
    stream.setAutoFormatting(true);
    stream.writeStartDocument();

    stream.writeStartElement("settings");
    stream.writeStartElement("config");

    stream.writeTextElement("mediaRoot", m_data.mediaRoot);

    foreach(QString auxMediaRoot, m_data.auxMediaRoots)
    {
        stream.writeTextElement("auxMediaRoot", auxMediaRoot);
    }
    
    if (!m_serialNumber.isEmpty())
    {
        stream.writeTextElement("serialNumber", m_serialNumber);
    }
    
    stream.writeEndElement(); // config
    stream.writeEndElement(); // settings

    stream.writeEndDocument();

    settingsFile.close();
}

bool Settings::isAllowChangeIP() const
{
    QReadLocker _lock(&m_RWLock);

    return m_data.allowChangeIP;
}

QString Settings::mediaRoot() const
{
    QReadLocker _lock(&m_RWLock);

    return m_data.mediaRoot;
}

void Settings::setMediaRoot(const QString& root)
{
    QWriteLocker _lock(&m_RWLock);

    m_data.mediaRoot = root;
}

QStringList Settings::auxMediaRoots() const
{
    QReadLocker _lock(&m_RWLock);

    return m_data.auxMediaRoots;
}

void Settings::addAuxMediaRoot(const QString& root)
{
    QWriteLocker _lock(&m_RWLock);

    // Do not add duplicates
    if (m_data.auxMediaRoots.indexOf(root) != -1)
        return;

    m_data.auxMediaRoots.append(root);
}

void Settings::setSerialNumber(const QString& serial)
{
    QWriteLocker _lock(&m_RWLock);

    SerialChecker serialChecker;
    if (serialChecker.isValidSerial(serial))
    {
        m_serialNumber = serial;
        m_haveValidSerialNumber = true;
    }
}

bool Settings::haveValidSerialNumber() const
{
    return m_haveValidSerialNumber;
}

/// Private methods. No internal synchronization needed.
void Settings::setAllowChangeIP(bool allow)
{
    m_data.allowChangeIP = allow;
}

void Settings::removeAuxMediaRoot(const QString& root)
{
    m_data.auxMediaRoots.removeAll(root);
}

void Settings::setAuxMediaRoots(const QStringList& auxMediaRoots)
{
    m_data.auxMediaRoots.clear();

    foreach(const QString& auxMediaRoot, auxMediaRoots)
    {
        addAuxMediaRoot(auxMediaRoot);
    }
}

void Settings::reset()
{
    m_data.allowChangeIP = false;
    m_haveValidSerialNumber = false;
}
