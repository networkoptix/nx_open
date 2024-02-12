// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QSet>

#include <nx/utils/impl_ptr.h>
#include <nx/utils/uuid.h>

namespace nx::core::access {

/**
 * Generic subject hierarchy tracker.
 *
 * Each subject has an id and may have parent subjects.
 * Subject are allowed to form a set of oriented graphs without loops.
 * Note: in current implementation the absence of loops is assert-checked, but not ensured.
 *
 * Subjects within a parent are called members.
 * Subjects with members are called groups.
 */
class NX_VMS_COMMON_API SubjectHierarchy: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    explicit SubjectHierarchy(QObject* parent = nullptr);
    virtual ~SubjectHierarchy() override;

    /** Check whether the specified subject exists in the hierarchy. */
    bool exists(const nx::Uuid& subject) const;

    /** Returns direct parents of the specified subject. */
    QSet<nx::Uuid> directParents(const nx::Uuid& subject) const;

    /** Returns direct members of the specified group. */
    QSet<nx::Uuid> directMembers(const nx::Uuid& group) const;

    /** Returns direct and indirect parents of specified subjects. */
    QSet<nx::Uuid> recursiveParents(const QSet<nx::Uuid>& subjects) const;

    /** Returns direct and indirect members of specified subjects. */
    QSet<nx::Uuid> recursiveMembers(const QSet<nx::Uuid>& subjects) const;

    /** Checks whether the specified subject is a direct member of the specified group. */
    bool isDirectMember(const nx::Uuid& subject, const nx::Uuid& group) const;

    /** Checks whether the subject is a direct or indirect member of any of specified parents. */
    bool isRecursiveMember(const nx::Uuid& subject, const QSet<nx::Uuid>& parents) const;

    /**
     * Checks whether the subject belongs to a set of specified subjects and their direct and
     * indirect members.
     */
    bool isWithin(const nx::Uuid& subject, const QSet<nx::Uuid>& subjects) const;

    QList<QSet<nx::Uuid>> findCycledGroupSets() const;
    QList<QSet<nx::Uuid>> findCycledGroupSetsForSpecificGroups(const QSet<nx::Uuid>& groups) const;

    /** Clears the entire hierarchy. Emits `reset` signal. */
    void clear();

    /** Copies a hierarchy. Emits `reset` signal. */
    SubjectHierarchy& operator=(const SubjectHierarchy& other);

signals:
    /** Emitted after the hierarchy has been changed. */
    void changed(
        const QSet<nx::Uuid>& added,
        const QSet<nx::Uuid>& removed,
        const QSet<nx::Uuid>& groupsWithChangedMembers,
        const QSet<nx::Uuid>& subjectsWithChangedParents);

    /** Emitted after the hierarchy has been completely replaced. */
    void reset();

protected:
    /**
     * Adds the specified subject to the hierarchy or updates its parents if it already exists.
     * If any of the specified parents doesn't exist in the hierarchy, then it's added along.
     * Returns false if nothing was changed, or true if the subject was added or updated.
     */
    bool addOrUpdate(const nx::Uuid& subject, const QSet<nx::Uuid>& parents);

    /**
     * Adds the specified subject to the hierarchy or updates its parents and members if it exists.
     * If any of the specified parents or members doesn't exist in the hierarchy, it's added along.
     * Returns false if nothing was changed, or true if the subject was added or updated.
     */
    bool addOrUpdate(
        const nx::Uuid& subject, const QSet<nx::Uuid>& parents, const QSet<nx::Uuid>& members);

    /**
     * Removes the specified subjects from the hierarchy.
     * If each removed subject has members, it stops being their parent.
     * Returns true if any subject was removed, or false if the hierarchy hasn't changed.
     */
    bool remove(const QSet<nx::Uuid>& subjects);
    bool remove(const nx::Uuid& subject);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::core::access
