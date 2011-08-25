#include "tagmanager.h"

#include <QtCore/QHash>
#include <QtCore/QMap>
#include <QtCore/QMutex>

#include "base/log.h"
#include "util.h"

#define TAGS_FILENAME "tags.txt"

typedef QMap<QString, int> TagsMap;
typedef QHash<QString, QStringList> ObjectTagsMap;

class TagManagerPrivate : public TagManager
{
public:
    TagManagerPrivate() : TagManager()
    {
        load();
    }

    void load();
    void save();

    QMutex m_mutex;

    TagsMap m_tags;
    ObjectTagsMap m_objectTags;
};

void TagManagerPrivate::load()
{
    const QString fileName = getDataDirectory() + QLatin1Char('/') + QLatin1String(TAGS_FILENAME);
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly))
        return;

    QTextStream stream(&file);
    while (!stream.atEnd())
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

void TagManagerPrivate::save()
{
    const QString fileName = getDataDirectory() + QLatin1Char('/') + QLatin1String(TAGS_FILENAME);
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly))
        return;

    QTextStream stream(&file);
    for (ObjectTagsMap::const_iterator it = m_objectTags.constBegin(); it != m_objectTags.constEnd(); ++it)
    {
        stream << it.key().length() << QLatin1Char(' ') << it.key()
               << QLatin1Char(' ') << it.value().join(QLatin1String(",")) << endl;
    }
}

Q_GLOBAL_STATIC(TagManagerPrivate, tagManager)


QStringList TagManager::allTags()
{
    TagManagerPrivate *manager = tagManager();

    QMutexLocker locker(&manager->m_mutex);

    return manager->m_tags.keys();
}

QStringList TagManager::objectTags(const QString &object)
{
    TagManagerPrivate *manager = tagManager();

    QMutexLocker locker(&manager->m_mutex);

    return manager->m_objectTags.value(object);
}

void TagManager::addObjectTag(const QString &object, const QString &tag)
{
    if (tag.trimmed().isEmpty())
        return;

    TagManagerPrivate *manager = tagManager();

    QMutexLocker locker(&manager->m_mutex);

    ObjectTagsMap::iterator objectTagIter = manager->m_objectTags.find(object);
    if (objectTagIter == manager->m_objectTags.end())
    {
        manager->m_objectTags[object] = QStringList(tag);
    }
    else
    {
        if (objectTagIter.value().contains(tag))
            return;

        objectTagIter.value() << tag;
    }

    ++manager->m_tags[tag];

    manager->save();
}

void TagManager::removeObjectTag(const QString &object, const QString &tag)
{
    if (tag.trimmed().isEmpty())
        return;

    TagManagerPrivate *manager = tagManager();

    QMutexLocker locker(&manager->m_mutex);

    ObjectTagsMap::iterator it = manager->m_objectTags.find(object);
    if (it == manager->m_objectTags.end())
        return;

    int nRemoved = it.value().removeAll(tag);
    if (nRemoved > 0)
    {
        manager->m_tags[tag] -= nRemoved;
        if (manager->m_tags[tag] <= 0)
            manager->m_tags.remove(tag);

        manager->save();
    }
}
