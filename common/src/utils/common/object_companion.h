#pragma once

#include <QtCore/QObject>
#include <QtCore/QScopedPointer>
#include <type_traits>
#include <utility>

/*
 *  A set of static methods to attach/detach a companion object to/from another object.
 *   A companion is identified by string id. Pointer to companion is stored in a dynamic property.
 */
class QnObjectCompanionManager
{
public:
    /** Get an attached companion: */
    static QObject* companion(QObject* parent, const char* id);

    /** Attach a companion if no other companion was attached: */
    static bool attachUnique(QObject* parent, QObject* companion, const char* id);

    /** Attach a companion and return previously attached companion: */
    static void attach(QObject* parent, QObject* companion, const char* id, QObject*& previousCompanion);

    /** Detach a companion and return it (nullptr if no companion was attached): */
    static QObject* detach(QObject* parent, const char* id);

    /** Detach and delete a companion (returns false if no companion was attached): */
    static bool uninstall(QObject* parent, const char* id);

protected:
    static QObject* getCompanion(QObject* parent, const QByteArray& internalId);
    static void setCompanion(QObject* parent, const QByteArray& internalId, QObject* companion);
    static void clearCompanion(QObject* parent, const QByteArray& internalId);

    static QByteArray companionId(const char* id);
};

/*
 * Generic object with static methods to create an instance and attach it as a companion to another object.
 */
template<class Base>
class QnObjectCompanion : public Base, public QnObjectCompanionManager
{
    static_assert(std::is_base_of<QObject, Base>::value, "QnObjectCompanion must be specialized with a class derived from QObject");

    QnObjectCompanion() = delete;

public:
    template<class ParentType>
    QnObjectCompanion(ParentType* parent) : Base(parent) {}

    /** Create and attach a companion if no other companion was attached: */
    template<class ParentType>
    static QnObjectCompanion<Base>* installUnique(ParentType* parent, const char* id)
    {
        const QByteArray internalId = QnObjectCompanionManager::companionId(id);
        if (QnObjectCompanionManager::getCompanion(parent, internalId))
            return nullptr;

        auto result = new QnObjectCompanion<Base>(parent);
        QnObjectCompanionManager::setCompanion(parent, internalId, result);
        return result;
    }

    /** Create and attach a companion and return previously attached companion: */
    template<class ParentType>
    static QnObjectCompanion<Base>* install(ParentType* parent, const char* id, QObject*& previousCompanion)
    {
        auto result = new QnObjectCompanion<Base>(parent);
        QnObjectCompanionManager::attach(parent, id, result, previousCompanion);
        return result;
    }
};
