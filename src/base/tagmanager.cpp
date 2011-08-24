#include "tagmanager.h"

#include <QtCore/QCoreApplication>

#include "base/log.h"
#include "util.h"

static const char* TAGS_FILENAME = "tags.txt";

Q_GLOBAL_STATIC(QMutex, theInstanceMutex)
static TagManager *tagManager = 0;

static void tagManagerDeleter()
{
    delete tagManager;
    tagManager = 0;
}

TagManager &TagManager::instance()
{
    if (tagManager)
        return *tagManager;

    QMutexLocker locker(theInstanceMutex());

    // Check second time for case when 1 or more threads waited on mutex in previous line
    if (!tagManager)
    {
        tagManager = new TagManager();
        qAddPostRoutine(tagManagerDeleter);
    }

    return *tagManager;
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
    QMutexLocker locker(&m_mutex);

    return m_tags.keys();
}

QStringList TagManager::listObjectTags(const QString& object) const
{
    QMutexLocker locker(&m_mutex);

    return m_objectTags.value(object);
}

void TagManager::addObjectTag(const QString& object, const QString& tag)
{
    if (tag.trimmed().isEmpty())
        return;

    QMutexLocker locker(&m_mutex);

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

    ++m_tags[tag];

    save();
}

void TagManager::removeObjectTag(const QString& object, const QString& tag)
{
    if (tag.trimmed().isEmpty())
        return;

    QMutexLocker locker(&m_mutex);

    int nRemoved = 0;
    {
        ObjectTagsType::iterator it = m_objectTags.find(object);
        if (it == m_objectTags.end())
            return;

        nRemoved = it.value().removeAll(tag);
    }

    if (nRemoved > 0)
    {
        m_tags[tag] -= nRemoved;
        if (m_tags[tag] <= 0)
            m_tags.remove(tag);

        save();
    }
}

void TagManager::load()
{
    QFile file(getDataDirectory() + QLatin1Char('/') + QLatin1String(TAGS_FILENAME));
    if (!file.open(QIODevice::ReadOnly))
        return;

    QTextStream stream(&file);
    for (;;)
    {
        QString line = stream.readLine().trimmed();
        if (line.isEmpty())
            continue;

        int pos = line.indexOf(QLatin1Char(' '));
        if (pos < 1)
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
        QStringList objectTags = line.mid(pos + 1 + deviceIdLength + 1).split(QLatin1Char(','), QString::SkipEmptyParts);
        objectTags.removeDuplicates();

        m_objectTags[objectId] = objectTags;

        foreach (const QString &tag, objectTags)
            ++m_tags[tag];
    }
}

void TagManager::save()
{
    QFile file(getDataDirectory() + QLatin1Char('/') + QLatin1String(TAGS_FILENAME));
    if (!file.open(QIODevice::WriteOnly))
        return;

    QTextStream stream(&file);
    for (ObjectTagsType::const_iterator it = m_objectTags.constBegin(); it != m_objectTags.constEnd(); ++it)
    {
        stream << it.key().length() << QLatin1Char(' ') << it.key()
               << QLatin1Char(' ') << it.value().join(QLatin1String(",")) << endl;
    }
}
