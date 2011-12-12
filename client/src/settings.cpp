#include "settings.h"

#include <QtCore/QSettings>

#include "license.h"
#include "utils/common/util.h"
#include "utils/common/log.h"

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
qreal global_decoration_opacity  = 0.3;
qreal global_decoration_max_opacity  = 0.7;

qreal global_base_scene_z_level = 1.0;

int global_grid_aparence_delay = 2000;

int global_opacity_change_period = 500;

Settings::Settings()
    : m_RWLock(QReadWriteLock::Recursive)
{
}

Settings& Settings::instance()
{
    static Settings settings;
    return settings;
}

bool Settings::isAfterFirstRun() const
{
    return m_data.afterFirstRun;
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
    m_data.maxVideoItems = data.maxVideoItems;
    m_data.downmixAudio = data.downmixAudio;
}

void Settings::load(const QString& fileName)
{
    QWriteLocker _lock(&m_RWLock);

    m_fileName = fileName;

    reset();

    QFile settingsFile(m_fileName);
    if (settingsFile.exists())
    {
        if (settingsFile.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            QXmlStreamReader xml(&settingsFile);
            while (!xml.atEnd())
            {
                xml.readNext();

                if (xml.isStartElement())
                {
                    if (xml.name() == QLatin1String("mediaRoot"))
                        m_data.mediaRoot = fromNativePath(xml.readElementText());
                    else if (xml.name() == QLatin1String("auxMediaRoot"))
                        m_data.auxMediaRoots.push_back(fromNativePath(xml.readElementText()));
                    else if (xml.name() == QLatin1String("afterFirstRun"))
                        m_data.afterFirstRun = xml.readElementText() == QLatin1String("true");
                    else if (xml.name() == QLatin1String("maxVideoItems"))
                        m_data.maxVideoItems = qBound(0, xml.readElementText().toInt(), 32);
                    else if (xml.name() == QLatin1String("downmixAudio"))
                        m_data.downmixAudio = xml.readElementText() == QLatin1String("true");
                }
            }

            if (xml.hasError())
                reset();
        }
        else
        {
            cl_log.log(QLatin1String("Can't open settings file"), cl_logERROR);
        }
    }
    else
    {
        cl_log.log(QLatin1String("No settings file"), cl_logERROR);
    }

    if (m_data.mediaRoot.isEmpty())
        m_data.mediaRoot = getMoviesDirectory() + QLatin1String("/EVE Media/");
}

void Settings::save()
{
    QWriteLocker _lock(&m_RWLock);

    QFile settingsFile(m_fileName);

    if (!settingsFile.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        cl_log.log(QLatin1String("Can't open settings file"), cl_logERROR);
        return;
    }

    QXmlStreamWriter stream(&settingsFile);
    stream.setAutoFormatting(true);
    stream.writeStartDocument();
    {
        stream.writeStartElement(QLatin1String("settings"));
        {
            stream.writeStartElement(QLatin1String("config"));
            {
                stream.writeTextElement(QLatin1String("mediaRoot"), QDir::toNativeSeparators(m_data.mediaRoot));

                foreach (const QString &auxMediaRoot, m_data.auxMediaRoots)
                    stream.writeTextElement(QLatin1String("auxMediaRoot"), QDir::toNativeSeparators(auxMediaRoot));

                stream.writeTextElement(QLatin1String("afterFirstRun"), QLatin1String("true"));

                stream.writeTextElement(QLatin1String("maxVideoItems"), QString::number(m_data.maxVideoItems));

                stream.writeTextElement(QLatin1String("downmixAudio"), m_data.downmixAudio ? QLatin1String("true") : QLatin1String("false"));
            }
            stream.writeEndElement(); // config
        }
        stream.writeEndElement(); // settings
    }
    stream.writeEndDocument();

    settingsFile.close();
}

int Settings::maxVideoItems() const
{
    QReadLocker _lock(&m_RWLock);

    return m_data.maxVideoItems;
}

bool Settings::downmixAudio() const
{
    QReadLocker _lock(&m_RWLock);

    return m_data.downmixAudio;
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

    m_data.auxMediaRoots.append(fromNativePath(root));
}

static inline Settings::ConnectionData readConnectionData(QSettings *settings)
{
    Settings::ConnectionData connection;
    connection.name = settings->value(QLatin1String("name")).toString();
    connection.url = settings->value(QLatin1String("url")).toUrl();

    return connection;
}

static inline void writeConnectionData(QSettings *settings, const Settings::ConnectionData &connection)
{
    settings->setValue(QLatin1String("name"), connection.name);
    settings->setValue(QLatin1String("url"), connection.url);
}

Settings::ConnectionData Settings::lastUsedConnection()
{
    ConnectionData connection;

    QSettings settings;
    settings.beginGroup(QLatin1String("AppServerConnections"));
    settings.beginGroup(QLatin1String("lastUsed"));
    connection = readConnectionData(&settings);
    settings.endGroup();
    settings.endGroup();

    return connection;
}

void Settings::setLastUsedConnection(const Settings::ConnectionData &connection)
{
    QSettings settings;
    settings.beginGroup(QLatin1String("AppServerConnections"));
    settings.beginGroup(QLatin1String("lastUsed"));
    writeConnectionData(&settings, connection);
    settings.endGroup();
    settings.endGroup();
}

QList<Settings::ConnectionData> Settings::connections()
{
    QList<ConnectionData> connections;

    QSettings settings;
    const int size = settings.beginReadArray(QLatin1String("AppServerConnections"));
    for (int i = 0; i < size; ++i) {
        settings.setArrayIndex(i);
        ConnectionData connection = readConnectionData(&settings);
        if (connection.url.isValid())
            connections.append(connection);
    }
    settings.endArray();

    return connections;
}

void Settings::setConnections(const QList<Settings::ConnectionData> &connections)
{
    ConnectionData lastUsed = lastUsedConnection();

    QSettings settings;
    settings.beginWriteArray(QLatin1String("AppServerConnections"));
    settings.remove(QLatin1String("")); // clear
    int i = 0;
    foreach (const ConnectionData &connection, connections) {
        if (!connection.url.isValid())
            continue;

        if (connection.name.trimmed().isEmpty()) {
            // special case: the last used connection
            lastUsed = connection;
        } else {
            settings.setArrayIndex(i++);
            writeConnectionData(&settings, connection);
        }
    }
    settings.endArray();

    setLastUsedConnection(lastUsed);
}

bool Settings::haveValidLicense()
{
    return QnLicense::defaultLicense().isValid();
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
    m_data.maxVideoItems = 0;
#ifdef Q_OS_DARWIN
    // mac version use SPDIF by default for multichannel audio
    m_data.downmixAudio = true;
#else
    m_data.downmixAudio = false;
#endif
    m_data.afterFirstRun = false;
    m_data.allowChangeIP = false;
}
