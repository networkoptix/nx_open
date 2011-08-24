#ifndef TAGMANAGER_H
#define TAGMANAGER_H

class TagManager
{
public:
    static TagManager& instance();

    QStringList listAllTags() const;

    QStringList listObjectTags(const QString& object) const;
    void addObjectTag(const QString& object, const QString& tag);
    void removeObjectTag(const QString& object, const QString& tag);

private:
    TagManager();
    TagManager(const TagManager&);

    void load();
    void save();

private:
    mutable QMutex m_mutex;

    typedef QMap<QString, int> TagsType;
    typedef QMap<QString, QStringList> ObjectTagsType;

    TagsType m_tags;
    ObjectTagsType m_objectTags;

private:
    static QMutex m_initMutex;
    static TagManager* m_instance;
};

#endif // TAGMANAGER_H
