// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "subject_hierarchy.h"

#include <QtCore/QHash>

#include <nx/utils/log/log.h>
#include <nx/utils/thread/mutex.h>

namespace nx::core::access {

struct SubjectHierarchy::Private
{
    QHash<QnUuid /*subject*/, QSet<QnUuid> /*groups*/> directParents;
    QHash<QnUuid /*group*/, QSet<QnUuid> /*subjects*/> directMembers;

    mutable nx::Mutex mutex;

    bool exists(const QnUuid& subject) const { return directParents.contains(subject); }

    QSet<QnUuid> recursiveParents(const QSet<QnUuid>& subjects) const;
    bool isRecursiveMember(const QnUuid& subject, const QSet<QnUuid>& groups) const;

private:
    void calculateRecursiveParents(const QnUuid& group, QSet<QnUuid>& result) const;

    bool isRecursiveMember(
        const QnUuid& subject, const QSet<QnUuid>& groups, QSet<QnUuid>& alreadyVisited) const;
};

SubjectHierarchy::SubjectHierarchy(QObject* parent):
    base_type(parent),
    d(new Private{})
{
}

SubjectHierarchy::~SubjectHierarchy()
{
    // Required here for forward-declared scoped pointer destruction.
}

QSet<QnUuid> SubjectHierarchy::directParents(const QnUuid& subject) const
{
    NX_MUTEX_LOCKER lk(&d->mutex);
    return d->directParents.value(subject);
}

QSet<QnUuid> SubjectHierarchy::directMembers(const QnUuid& group) const
{
    NX_MUTEX_LOCKER lk(&d->mutex);
    return d->directMembers.value(group);
}

QSet<QnUuid> SubjectHierarchy::recursiveParents(const QSet<QnUuid>& subjects) const
{
    NX_MUTEX_LOCKER lk(&d->mutex);
    return d->recursiveParents(subjects);
}

QSet<QnUuid> SubjectHierarchy::Private::recursiveParents(const QSet<QnUuid>&subjects) const
{
    QSet<QnUuid> result;
    for (const auto subject: subjects)
        calculateRecursiveParents(subject, result);

    return result;
}

void SubjectHierarchy::Private::calculateRecursiveParents(
    const QnUuid& group, QSet<QnUuid>& result) const
{
    for (const auto parent: directParents.value(group))
    {
        if (result.contains(parent))
            continue;

        result.insert(parent);
        calculateRecursiveParents(parent, result);
    }
}

bool SubjectHierarchy::isDirectMember(const QnUuid& subject, const QnUuid& group) const
{
    return directParents(subject).contains(group);
}

bool SubjectHierarchy::isRecursiveMember(const QnUuid& subject, const QSet<QnUuid>& parents) const
{
    NX_MUTEX_LOCKER lk(&d->mutex);
    return d->isRecursiveMember(subject, parents);
}

bool SubjectHierarchy::Private::isRecursiveMember(
    const QnUuid& subject, const QSet<QnUuid>& groups) const
{
    QSet<QnUuid> alreadyVisited;
    return isRecursiveMember(subject, groups, alreadyVisited);
}

bool SubjectHierarchy::Private::isRecursiveMember(
    const QnUuid& subject, const QSet<QnUuid>& groups, QSet<QnUuid>& alreadyVisited) const
{
    alreadyVisited.insert(subject);

    for (const auto& parent: directParents.value(subject))
    {
        if (alreadyVisited.contains(parent))
            continue;

        if (groups.contains(parent))
            return true;

        if (isRecursiveMember(parent, groups, alreadyVisited))
            return true;
    }

    return false;
}

bool SubjectHierarchy::exists(const QnUuid& subject) const
{
    NX_MUTEX_LOCKER lk(&d->mutex);
    return d->exists(subject);
}

void SubjectHierarchy::clear()
{
    NX_DEBUG(this, "Clearing hierarchy");

    NX_MUTEX_LOCKER lk(&d->mutex);
    d->directParents.clear();
    d->directMembers.clear();
    lk.unlock();
    emit reset();
}

SubjectHierarchy& SubjectHierarchy::operator=(const SubjectHierarchy& other)
{
    NX_DEBUG(this, "Assigning another hierarchy");

    NX_MUTEX_LOCKER lk1(&d->mutex);
    NX_MUTEX_LOCKER lk2(&other.d->mutex);
    d->directParents = other.d->directParents;
    d->directMembers = other.d->directMembers;
    lk2.unlock();
    lk1.unlock();
    emit reset();
    return *this;
}

bool SubjectHierarchy::addOrUpdate(const QnUuid& subject, const QSet<QnUuid>& parents)
{
    NX_MUTEX_LOCKER lk(&d->mutex);
    const bool existing = d->exists(subject);

    if (existing)
        NX_DEBUG(this, "Setting for %1 the following parents: %2", subject, parents);
    else
        NX_DEBUG(this, "Adding subject %1 with the following parents: %2", subject, parents);

    auto& currentParents = d->directParents[subject];

    const auto removedFrom = currentParents - parents;
    auto addedTo = parents - currentParents;

    if (!NX_ASSERT(!parents.contains(subject), "A subject cannot be a parent to itself"))
        addedTo.remove(subject);

    NX_VERBOSE(this, "Subject %1 is being removed from %2 and added to %3",
        subject, removedFrom, addedTo);

    if (existing && removedFrom.empty() && addedTo.empty())
        return false; //< No actual change.

    const QSet<QnUuid> subjectAsSet{subject};
    for (const auto& id: parents)
    {
        if (id == subject || d->isRecursiveMember(id, subjectAsSet))
            NX_WARNING(this, "Circular dependency detected for %1", subject.toString());
    }

    currentParents = parents;

    for (const auto& group: removedFrom)
        d->directMembers[group].remove(subject);

    QSet<QnUuid> newlyAdded;
    for (const auto& group: addedTo)
    {
        if (!d->exists(group))
        {
            d->directParents.insert(group, {});
            newlyAdded.insert(group);
        }

        d->directMembers[group].insert(subject);
    }

    const auto subjectsWithChangedParents = existing ? subjectAsSet : QSet<QnUuid>();
    const auto groupsWithChangedMembers = removedFrom + (addedTo - newlyAdded);
    if (!existing)
        newlyAdded.insert(subject);

    lk.unlock();

    NX_VERBOSE(this,
        "Newly added: %1, groups with changed members: %2, subjects with changed parents: %3",
        newlyAdded, groupsWithChangedMembers, subjectsWithChangedParents);

    emit changed(
        newlyAdded,
        /*removed*/ {},
        groupsWithChangedMembers,
        subjectsWithChangedParents);

    return true;
}

bool SubjectHierarchy::addOrUpdate(
    const QnUuid& subject, const QSet<QnUuid>& parents, const QSet<QnUuid>& members)
{
    NX_MUTEX_LOCKER lk(&d->mutex);
    const bool existing = d->exists(subject);

    if (existing)
    {
        NX_DEBUG(this, "Setting for %1 the following parents: %2, members: %3",
            subject, parents, members);
    }
    else
    {
        NX_DEBUG(this, "Adding subject %1 with the following parents: %2, members: %3",
            subject, parents, members);
    }

    auto& currentParents = d->directParents[subject];
    auto& currentMembers = d->directMembers[subject];

    if (currentParents == parents && currentMembers == members)
        return false;

    const auto memberRemovedFrom = currentParents - parents;
    auto memberAddedTo = parents - currentParents;

    const auto parentRemovedFrom = currentMembers - members;
    auto parentAddedTo = members - currentMembers;

    if (!NX_ASSERT(!parents.contains(subject), "A subject cannot be a parent to itself"))
        memberAddedTo.remove(subject);

    if (!NX_ASSERT(!members.contains(subject), "A subject cannot be a member of itself"))
        parentAddedTo.remove(subject);

    NX_VERBOSE(this, "Subject %1 is being removed from %2 and added to %3",
        subject, memberRemovedFrom, memberAddedTo);

    NX_VERBOSE(this, "Subject %1 members %2 are being removed, %3 added",
        subject, parentRemovedFrom, parentAddedTo);

    currentParents = parents;
    currentMembers = members;

    for (const auto& group: memberRemovedFrom)
        d->directMembers[group].remove(subject);

    QSet<QnUuid> newlyAddedParents;
    for (const auto& group: memberAddedTo)
    {
        if (!d->exists(group))
        {
            d->directParents.insert(group, {});
            newlyAddedParents.insert(group);
        }

        d->directMembers[group].insert(subject);
    }

    for (const auto& member: parentRemovedFrom)
        d->directParents[member].remove(subject);

    QSet<QnUuid> newlyAddedMembers;
    for (const auto& member: parentAddedTo)
    {
        if (!d->exists(member))
        {
            d->directMembers.insert(member, {});
            newlyAddedMembers.insert(member);
        }

        d->directParents[member].insert(subject);
    }

    if (d->isRecursiveMember(subject, members))
        NX_WARNING(this, "Circular dependency detected for %1", subject.toString());

    lk.unlock();

    const auto changedSubjectItself = existing ? QSet<QnUuid>{subject} : QSet<QnUuid>();

    const auto subjectsWithChangedParents = parentRemovedFrom + (parentAddedTo - newlyAddedMembers)
        + changedSubjectItself;

    const auto groupsWithChangedMembers = memberRemovedFrom + (memberAddedTo - newlyAddedParents)
        + changedSubjectItself;

    if (!existing)
        newlyAddedParents.insert(subject);

    const auto newlyAdded = newlyAddedParents + newlyAddedMembers;

    NX_VERBOSE(this,
        "Newly added: %1, groups with changed members: %2, subjects with changed parents: %3",
        newlyAdded, groupsWithChangedMembers, subjectsWithChangedParents);

    emit changed(
        newlyAdded,
        /*removed*/{},
        groupsWithChangedMembers,
        subjectsWithChangedParents);

    return true;
}

bool SubjectHierarchy::remove(const QSet<QnUuid>& subjects)
{
    NX_DEBUG(this, "Removing %1 from the hierarchy", subjects);

    NX_MUTEX_LOCKER lk(&d->mutex);

    QSet<QnUuid> groupsWithChangedMembers;
    QSet<QnUuid> subjectsWithChangedParents;
    QSet<QnUuid> removed;

    for (const auto subject: subjects)
    {
        if (!d->exists(subject))
            continue;

        const auto parentOf = d->directMembers.value(subject);
        const auto removedFrom = d->directParents.value(subject);

        for (const auto& group: removedFrom)
            d->directMembers[group].remove(subject);

        for (const auto& member: parentOf)
            d->directParents[member].remove(subject);

        d->directParents.remove(subject);
        d->directMembers.remove(subject);

        groupsWithChangedMembers += removedFrom;
        subjectsWithChangedParents += parentOf;
        removed.insert(subject);
    }

    lk.unlock();

    NX_VERBOSE(this, "Actually removed: %1", removed);

    if (removed.empty())
        return false;

    groupsWithChangedMembers -= subjects;
    subjectsWithChangedParents -= subjects;

    NX_VERBOSE(this, "Groups with changed members: %1, subjects with changed parents: %2",
        groupsWithChangedMembers, subjectsWithChangedParents);

    emit changed(/*added*/ {}, removed, groupsWithChangedMembers, subjectsWithChangedParents);
    return true;
}

bool SubjectHierarchy::remove(const QnUuid& subject)
{
    return remove(QSet<QnUuid>({subject}));
}

} // namespace nx::core::access
