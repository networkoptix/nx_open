// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "test_subject_hierarchy.h"

namespace nx::core::access {
namespace test {

TestSubjectHierarchy::TestSubjectHierarchy(QObject* parent):
    base_type(parent)
{
}

void TestSubjectHierarchy::addOrUpdate(const QString& subject, const Names& parents)
{
    base_type::addOrUpdate(ensureId(subject), ensureIds(parents));
}

void TestSubjectHierarchy::addOrUpdate(
    const QString& subject, const Names& parents, const Names& members)
{
    base_type::addOrUpdate(ensureId(subject), ensureIds(parents), ensureIds(members));
}

void TestSubjectHierarchy::remove(const Names& subjects)
{
    base_type::remove(ensureIds(subjects));
}

void TestSubjectHierarchy::remove(const QString& subject)
{
    base_type::remove(ensureId(subject));
}

TestSubjectHierarchy::Names TestSubjectHierarchy::directParents(const QString& subject) const
{
    return names(base_type::directParents(ensureId(subject)));
}

TestSubjectHierarchy::Names TestSubjectHierarchy::directMembers(const QString& group) const
{
    return names(base_type::directMembers(ensureId(group)));
}

TestSubjectHierarchy::Names TestSubjectHierarchy::recursiveParents(const QString& group) const
{
    return recursiveParents(Names{group});
}

TestSubjectHierarchy::Names TestSubjectHierarchy::recursiveParents(const Names& groups) const
{
    return names(base_type::recursiveParents(ensureIds(groups)));
}

bool TestSubjectHierarchy::isDirectMember(const QString& member, const QString& group) const
{
    return base_type::isDirectMember(ensureId(member), ensureId(group));
}

bool TestSubjectHierarchy::isRecursiveMember(const QString& member, const QString& group) const
{
    return isRecursiveMember(member, Names{group});
}

bool TestSubjectHierarchy::isRecursiveMember(
    const QString& member, const Names& groups) const
{
    return base_type::isRecursiveMember(ensureId(member), ensureIds(groups));
}

bool TestSubjectHierarchy::exists(const QString& subject) const
{
    return base_type::exists(ensureId(subject));
}

void TestSubjectHierarchy::clear()
{
    base_type::clear();
    nx::vms::common::test::NameToId::clear();
}

} // namespace test
} // namespace nx::core::access
