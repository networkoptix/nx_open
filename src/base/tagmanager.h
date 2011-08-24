#ifndef TAGMANAGER_H
#define TAGMANAGER_H

#include <QtCore/QMutex>

class TagManager
{
public:
    static TagManager& instance();

    QStringList listAllTags() const;

    QStringList listObjectTags(const QString& object) const;
    void addObjectTag(const QString& object, const QString& tag);
    void removeObjectTag(const QString& object, const QString& tag);

protected:
    TagManager();

    void load();
    void save();

private:
    Q_DISABLE_COPY(TagManager);

    mutable QMutex m_mutex;

    typedef QMap<QString, int> TagsType;
    typedef QMap<QString, QStringList> ObjectTagsType;

    TagsType m_tags;
    ObjectTagsType m_objectTags;
};

#endif // TAGMANAGER_H
