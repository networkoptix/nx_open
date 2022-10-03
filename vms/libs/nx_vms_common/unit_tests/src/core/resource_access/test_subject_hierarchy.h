// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QHash>
#include <QtCore/QString>

#include <core/resource_access/subject_hierarchy.h>
#include <nx/vms/common/test_support/utils/name_to_id.h>

namespace nx::core::access {
namespace test {

class TestSubjectHierarchy:
    public SubjectHierarchy,
    public nx::vms::common::test::NameToId
{
    using base_type = SubjectHierarchy;

public:
    explicit TestSubjectHierarchy(QObject* parent = nullptr);

    void addOrUpdate(const QString& subject, const Names& parents);
    void addOrUpdate(const QString& subject, const Names& parents, const Names& members);
    void remove(const Names& subjects);
    void remove(const QString& subject);

    Names directParents(const QString& subject) const;
    Names directMembers(const QString& group) const;

    Names recursiveParents(const QString& group) const;
    Names recursiveParents(const Names& groups) const;

    bool isDirectMember(const QString& member, const QString& group) const;
    bool isRecursiveMember(const QString& member, const Names& groups) const;
    bool isRecursiveMember(const QString& member, const QString& group) const;

    bool exists(const QString& subject) const;

    void clear();
};

} // namespace test
} // namespace nx::core::access
