#ifndef TAGMANAGER_H
#define TAGMANAGER_H

class TagManager
{
public:
    static QStringList allTags();

    static QStringList objectTags(const QString &object);
    static void setObjectTags(const QString &object, const QStringList &tags);

    static void addObjectTag(const QString &object, const QString &tag);
    static void removeObjectTag(const QString &object, const QString &tag);

protected:
    inline TagManager() {}
};

#endif // TAGMANAGER_H
