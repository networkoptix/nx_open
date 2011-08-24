#include "base/log.h"
#include "util.h"

#include "tagmanager.h"

static const char* TAGS_FILENAME = "tags.txt";

QMutex TagManager::m_initMutex;
TagManager* TagManager::m_instance;

TagManager& TagManager::instance()
{
    if (m_instance)
        return *m_instance;

    QMutexLocker _lock(&m_initMutex);

    // Check second time for case when 2 or more threads waited on mutex in previous line
    if (m_instance)
    {
        return *m_instance;
    }
    else
    {
        m_instance = new TagManager();
        return *m_instance;
    }
}

TagManager::TagManager()
{
    load();
}

TagManager::TagManager(const TagManager&)
{
}

QStringList TagManager::listAllTags() const
{
    QMutexLocker _lock(&m_mutex);

    return m_tags.keys();
}

QStringList TagManager::listObjectTags(const QString& object) const
{
    QMutexLocker _lock(&m_mutex);

    ObjectTagsType::const_iterator it = m_objectTags.find(object);
    if (it == m_objectTags.end())
        return QStringList();
    else
        return it.value();
}

void TagManager::addObjectTag(const QString& object, const QString& tag)
{
    if (tag.trimmed().isEmpty())
        return;

    QMutexLocker _lock(&m_mutex);

    ObjectTagsType::iterator objectTagIter = m_objectTags.find(object);
    if (objectTagIter == m_objectTags.end())
    {
        m_objectTags[object] = QStringList(tag);
    }
    else
    {
        if (objectTagIter.value().contains(tag))
            return;

        objectTagIter.value() << tag;
    }

    TagsType::iterator tagIter = m_tags.find(tag);
    if (tagIter == m_tags.end())
        m_tags[tag] = 1;
    else
        ++(tagIter.value());

    save();
}

void TagManager::removeObjectTag(const QString& object, const QString& tag)
{
    QMutexLocker _lock(&m_mutex);

    int nRemoved = 0;

    {
        ObjectTagsType::iterator it = m_objectTags.find(object);
        if (it == m_objectTags.end())
            return;

        nRemoved = it.value().removeAll(tag);
    }

    {
        TagsType::iterator it = m_tags.find(tag);
        it.value() -= nRemoved;
    }

    save();
}

void TagManager::load()
{
    QString dataLocation = getDataDirectory();

    QFile file(dataLocation + QLatin1String("/") + TAGS_FILENAME);
    if (!file.open (QIODevice::ReadOnly))
        return;

    QTextStream stream (&file);
    QString line;

    for (;;)
    {
        line = stream.readLine().trimmed();

        if (line.isNull())
            break;

        if (line.isEmpty())
            continue;

        int pos = line.indexOf(' ');
        if (pos < 1 || pos > line.length())
        {
            cl_log.log("Can't parse tags file. Can't find device length field.", cl_logERROR);
            break;
        }

        int deviceIdLength = line.left(pos).toInt();
        if (deviceIdLength == 0 || deviceIdLength > line.length() - pos - 1)
        {
            cl_log.log("Can't parse tags file. Invalid device length.", cl_logERROR);
            break;
        }

        QString objectId = line.mid(pos + 1, deviceIdLength);
        QStringList objectTags = line.mid(pos + 1 + deviceIdLength + 1).split(",");


        objectTags.removeAll("");
        objectTags.removeDuplicates();

        m_objectTags[objectId] = objectTags;

        foreach(QString tag, objectTags)
        {
            if (m_tags.contains(tag))
                m_tags[tag] += 1;
            else
                m_tags[tag] = 1;
        }
    }
}

void TagManager::save()
{
    QString dataLocation = getDataDirectory();

    QFile file(dataLocation + QLatin1String("/") + TAGS_FILENAME);
    if (!file.open (QIODevice::WriteOnly))
        return;

    QTextStream stream (&file);
    for (ObjectTagsType::const_iterator it = m_objectTags.begin(); it != m_objectTags.end(); ++it)
    {
        stream << it.key().length() << " " << it.key() << " " << it.value().join(",") << endl;
    }
}
