// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <memory>

#include "test_subject_hierarchy.h"

namespace nx::core::access {
namespace test {

class SubjectHierarchyTest:
    public ::testing::Test,
    public TestSubjectHierarchy
{
public:
    Names lastAdded;
    Names lastRemoved;
    Names lastChangedGroups;
    Names lastChangedMembers;

protected:
    SubjectHierarchyTest()
    {
        connect(this, &TestSubjectHierarchy::changed,
            [this](
                const QSet<nx::Uuid>& added,
                const QSet<nx::Uuid>& removed,
                const QSet<nx::Uuid>& groupsWithChangedMembers,
                const QSet<nx::Uuid>& subjectsWithChangedParents)
            {
                lastAdded = names(added);
                lastRemoved = names(removed);
                lastChangedGroups = names(groupsWithChangedMembers);
                lastChangedMembers = names(subjectsWithChangedParents);
            });
    }

    virtual void TearDown() override
    {
        clear();
        lastAdded.clear();
        lastRemoved.clear();
        lastChangedGroups.clear();
        lastChangedMembers.clear();
    }
};

TEST_F(SubjectHierarchyTest, addRemoveUpdate)
{
    addOrUpdate("Group 1", Names({}));
    ASSERT_EQ(lastAdded, Names({"Group 1"}));
    ASSERT_EQ(lastRemoved, Names({}));
    ASSERT_EQ(lastChangedGroups, Names({}));
    ASSERT_EQ(lastChangedMembers, Names({}));
    ASSERT_EQ(directParents("Group 1"), Names({}));
    ASSERT_EQ(directMembers("Group 1"), Names({}));

    addOrUpdate("Group 2", Names({"Group 1"}));
    ASSERT_EQ(lastAdded, Names({"Group 2"}));
    ASSERT_EQ(lastRemoved, Names({}));
    ASSERT_EQ(lastChangedGroups, Names({"Group 1"}));
    ASSERT_EQ(lastChangedMembers, Names{});
    ASSERT_EQ(directParents("Group 1"), Names({}));
    ASSERT_EQ(directMembers("Group 1"), Names({"Group 2"}));
    ASSERT_EQ(directParents("Group 2"), Names({"Group 1"}));
    ASSERT_EQ(directMembers("Group 2"), Names({}));

    addOrUpdate("User 1", Names({"Group 2"}));
    ASSERT_EQ(lastAdded, Names({"User 1"}));
    ASSERT_EQ(lastRemoved, Names({}));
    ASSERT_EQ(lastChangedGroups, Names({"Group 2"}));
    ASSERT_EQ(lastChangedMembers, Names({}));
    ASSERT_EQ(directParents("Group 1"), Names({}));
    ASSERT_EQ(directMembers("Group 1"), Names({"Group 2"}));
    ASSERT_EQ(directParents("Group 2"), Names({"Group 1"}));
    ASSERT_EQ(directMembers("Group 2"), Names({"User 1"}));
    ASSERT_EQ(directParents("User 1"), Names({"Group 2"}));
    ASSERT_EQ(directMembers("User 1"), Names({}));

    addOrUpdate("User 2", Names({"Group 2"}));
    ASSERT_EQ(lastAdded, Names({"User 2"}));
    ASSERT_EQ(lastRemoved, Names({}));
    ASSERT_EQ(lastChangedGroups, Names({"Group 2"}));
    ASSERT_EQ(lastChangedMembers, Names({}));
    ASSERT_EQ(directParents("Group 1"), Names({}));
    ASSERT_EQ(directMembers("Group 1"), Names({"Group 2"}));
    ASSERT_EQ(directParents("Group 2"), Names({"Group 1"}));
    ASSERT_EQ(directMembers("Group 2"), Names({"User 1", "User 2"}));
    ASSERT_EQ(directParents("User 1"), Names({"Group 2"}));
    ASSERT_EQ(directMembers("User 1"), Names({}));
    ASSERT_EQ(directParents("User 2"), Names({"Group 2"}));
    ASSERT_EQ(directMembers("User 2"), Names({}));

    //     Group 1
    //        |
    //        v
    //     Group 2
    //        |
    //   +----+----+
    //   |         |
    //   v         v
    // User 1   User 2

    remove(Names({"Group 2", "User 2"}));
    ASSERT_EQ(lastAdded, Names({}));
    ASSERT_EQ(lastRemoved, Names({"Group 2", "User 2"}));
    ASSERT_EQ(lastChangedGroups, Names({"Group 1"}));
    ASSERT_EQ(lastChangedMembers, Names({"User 1"}));
    ASSERT_EQ(directParents("Group 1"), Names({}));
    ASSERT_EQ(directMembers("Group 1"), Names({}));
    ASSERT_EQ(directParents("User 1"), Names({}));
    ASSERT_EQ(directMembers("User 1"), Names({}));

    addOrUpdate("User 1", Names({"Group 1"}));
    ASSERT_EQ(lastAdded, Names({}));
    ASSERT_EQ(lastRemoved, Names({}));
    ASSERT_EQ(lastChangedGroups, Names({"Group 1"}));
    ASSERT_EQ(lastChangedMembers, Names({"User 1"}));
    ASSERT_EQ(directParents("Group 1"), Names({}));
    ASSERT_EQ(directMembers("Group 1"), Names({"User 1"}));
    ASSERT_EQ(directParents("User 1"), Names({"Group 1"}));
    ASSERT_EQ(directMembers("User 1"), Names({}));
}

TEST_F(SubjectHierarchyTest, multipleMembership)
{
    addOrUpdate("Group 1", {});
    addOrUpdate("Group 2", {});
    addOrUpdate("User 1", {"Group 1", "Group 2"});
    addOrUpdate("User 2", {"Group 2"});

    // Group 1    Group 2
    //   |         |  |
    //   +----+----+  |
    //        |       |
    //        v       v
    //     User 1   User 2

    ASSERT_EQ(directParents("User 1"), Names({"Group 1", "Group 2"}));
    ASSERT_EQ(directParents("User 2"), Names({"Group 2"}));
    ASSERT_EQ(directParents("Group 1"), Names({}));
    ASSERT_EQ(directParents("Group 2"), Names({}));

    ASSERT_EQ(directMembers("User 1"), Names({}));
    ASSERT_EQ(directMembers("User 2"), Names({}));
    ASSERT_EQ(directMembers("Group 1"), Names({"User 1"}));
    ASSERT_EQ(directMembers("Group 2"), Names({"User 1", "User 2"}));

    remove("Group 2");
    ASSERT_EQ(directParents("User 1"), Names({"Group 1"}));
    ASSERT_EQ(directParents("User 2"), Names({}));
    ASSERT_EQ(directMembers("Group 1"), Names({"User 1"}));
    ASSERT_EQ(lastRemoved, Names({"Group 2"}));
    ASSERT_EQ(lastChangedGroups, Names({}));
    ASSERT_EQ(lastChangedMembers, Names({"User 1", "User 2"}));
}

TEST_F(SubjectHierarchyTest, isDirectMember)
{
    addOrUpdate("Group 1", {});
    addOrUpdate("Group 2", {"Group 1"});
    addOrUpdate("Group 3", {});
    addOrUpdate("User 1", {"Group 2", "Group 3"});

    // Group 1
    //   |
    //   v
    // Group 2   Group 3
    //   |         |
    //   +----+----+
    //        |
    //        v
    //      User 1

    ASSERT_TRUE(isDirectMember("Group 2", "Group 1"));
    ASSERT_FALSE(isDirectMember("Group 3", "Group 1"));
    ASSERT_FALSE(isDirectMember("Group 2", "Group 3"));
    ASSERT_FALSE(isDirectMember("Group 3", "Group 2"));
    ASSERT_FALSE(isDirectMember("Group 1", "Group 2"));
    ASSERT_FALSE(isDirectMember("Group 1", "Group 3"));
    ASSERT_TRUE(isDirectMember("User 1", "Group 2"));
    ASSERT_TRUE(isDirectMember("User 1", "Group 3"));
    ASSERT_FALSE(isDirectMember("User 1", "Group 1"));

    // A subject is not a direct member of itself.
    ASSERT_FALSE(isDirectMember("User 1", "User 1"));
    ASSERT_FALSE(isDirectMember("Group 1", "Group 1"));
    ASSERT_FALSE(isDirectMember("Group 2", "Group 2"));
}

TEST_F(SubjectHierarchyTest, isRecursiveMember)
{
    addOrUpdate("Group 1", {});
    addOrUpdate("Group 2", {"Group 1"});
    addOrUpdate("Group 3", {"Group 1"});
    addOrUpdate("User 1", {"Group 2", "Group 3"});
    addOrUpdate("User 2", {"Group 3"});

    //      Group 1
    //        |
    //   +----+----+
    //   |         |
    //   v         v
    // Group 2    Group 3
    //   |         |  |
    //   +----+----+  |
    //        |       |
    //        v       v
    //     User 1   User 2

    ASSERT_TRUE(isRecursiveMember("User 1", "Group 1"));
    ASSERT_TRUE(isRecursiveMember("User 1", "Group 2"));
    ASSERT_TRUE(isRecursiveMember("User 1", "Group 3"));

    ASSERT_TRUE(isRecursiveMember("User 2", "Group 1"));
    ASSERT_FALSE(isRecursiveMember("User 2", "Group 2"));
    ASSERT_TRUE(isRecursiveMember("User 2", "Group 3"));

    ASSERT_FALSE(isRecursiveMember("Group 1", "Group 2"));
    ASSERT_FALSE(isRecursiveMember("Group 1", "Group 3"));
    ASSERT_FALSE(isRecursiveMember("Group 2", "Group 3"));
    ASSERT_FALSE(isRecursiveMember("Group 3", "Group 2"));

    // A subject is not a recursive member of itself.
    ASSERT_FALSE(isRecursiveMember("User 1", "User 1"));
    ASSERT_FALSE(isRecursiveMember("Group 1", "Group 1"));
    ASSERT_FALSE(isRecursiveMember("Group 2", "Group 2"));
}

TEST_F(SubjectHierarchyTest, recursiveParents)
{
    addOrUpdate("Group 1", {});
    addOrUpdate("Group 2", {"Group 1"});
    addOrUpdate("Group 3", {"Group 1"});
    addOrUpdate("User 1", {"Group 2", "Group 3"});
    addOrUpdate("User 2", {"Group 3"});

    //      Group 1
    //        |
    //   +----+----+
    //   |         |
    //   v         v
    // Group 2    Group 3
    //   |         |  |
    //   +----+----+  |
    //        |       |
    //        v       v
    //     User 1   User 2

    ASSERT_EQ(recursiveParents("Group 1"), Names());
    ASSERT_EQ(recursiveParents("Group 2"), Names({"Group 1"}));
    ASSERT_EQ(recursiveParents("Group 3"), Names({"Group 1"}));
    ASSERT_EQ(recursiveParents("User 1"), Names({"Group 1", "Group 2", "Group 3"}));
    ASSERT_EQ(recursiveParents("User 2"), Names({ "Group 1", "Group 3" }));

    ASSERT_EQ(recursiveParents(Names{"User 1", "User 2"}),
        Names({"Group 1", "Group 2", "Group 3"}));
}

TEST_F(SubjectHierarchyTest, automaticallyAddParents)
{
    // When a subject is added or updated with parents that are not yet in the hierarchy,
    // they are added as well (without own parents).

    addOrUpdate("User 1", {"Group 2", "Group 3"});

    // Group 2   Group 3
    //   |         |
    //   +----+----+
    //        |
    //        v
    //     User 1

    ASSERT_EQ(lastAdded, Names({"User 1", "Group 2", "Group 3"}));
    ASSERT_EQ(lastChangedGroups, Names({}));
    ASSERT_EQ(lastChangedMembers, Names({}));

    ASSERT_TRUE(exists("User 1"));
    ASSERT_TRUE(exists("Group 2"));
    ASSERT_TRUE(exists("Group 3"));
    ASSERT_FALSE(exists("Group 1"));

    ASSERT_EQ(directParents("User 1"), Names({"Group 2", "Group 3"}));
    ASSERT_EQ(directMembers("Group 2"), Names({"User 1"}));
    ASSERT_EQ(directMembers("Group 3"), Names({"User 1"}));

    addOrUpdate("Group 2", {"Group 1"});

    // Group 1
    //   |
    //   v
    // Group 2   Group 3
    //   |         |
    //   +----+----+
    //        |
    //        v
    //     User 1

    ASSERT_EQ(lastAdded, Names({"Group 1"}));
    ASSERT_EQ(lastChangedGroups, Names({}));
    ASSERT_EQ(lastChangedMembers, Names({"Group 2"}));

    ASSERT_TRUE(exists("User 1"));
    ASSERT_TRUE(exists("Group 2"));
    ASSERT_TRUE(exists("Group 3"));
    ASSERT_TRUE(exists("Group 1"));

    ASSERT_EQ(directParents("Group 2"), Names({"Group 1"}));
    ASSERT_EQ(directMembers("Group 1"), Names({"Group 2"}));
}

TEST_F(SubjectHierarchyTest, setFullRelations)
{
    addOrUpdate("Group 1", {}, {"Group 2"});
    //      Group 1
    //         |
    //      Group 2

    ASSERT_EQ(lastAdded, Names({"Group 1", "Group 2"}));
    ASSERT_EQ(lastRemoved, Names({}));
    ASSERT_EQ(lastChangedGroups, Names({}));
    ASSERT_EQ(lastChangedMembers, Names({}));
    ASSERT_EQ(directParents("Group 1"), Names({}));
    ASSERT_EQ(directMembers("Group 1"), Names({"Group 2"}));
    ASSERT_EQ(directParents("Group 2"), Names({"Group 1"}));
    ASSERT_EQ(directMembers("Group 2"), Names({}));

    addOrUpdate("Group 3", {"Group 1"}, {"User 1", "User 2"});
    //      Group 1
    //        |
    //   +----+----+
    //   |         |
    //   v         v
    // Group 2   Group 3
    //             |
    //         +---+---+
    //         |       |
    //         v       v
    //      User 1   User 2

    ASSERT_EQ(lastAdded, Names({"Group 3", "User 1", "User 2"}));
    ASSERT_EQ(lastRemoved, Names({}));
    ASSERT_EQ(lastChangedGroups, Names({"Group 1"}));
    ASSERT_EQ(lastChangedMembers, Names({}));
    ASSERT_EQ(directParents("Group 1"), Names({}));
    ASSERT_EQ(directMembers("Group 1"), Names({"Group 2", "Group 3"}));
    ASSERT_EQ(directParents("Group 2"), Names({"Group 1"}));
    ASSERT_EQ(directMembers("Group 2"), Names({}));
    ASSERT_EQ(directParents("Group 3"), Names({"Group 1"}));
    ASSERT_EQ(directMembers("Group 3"), Names({"User 1", "User 2"}));
    ASSERT_EQ(directParents("User 1"), Names({"Group 3"}));
    ASSERT_EQ(directMembers("User 1"), Names({}));
    ASSERT_EQ(directParents("User 2"), Names({"Group 3"}));
    ASSERT_EQ(directMembers("User 2"), Names({}));

    addOrUpdate("Group 2", {"Group 4"}, {"User 1"});
    // Group 4   Group 1
    //   |          |
    //   v          v
    // Group 2   Group 3
    //   |         |  |
    //   +----+----+  |
    //        |       |
    //        v       v
    //     User 1   User 2

    ASSERT_EQ(lastAdded, Names({"Group 4"}));
    ASSERT_EQ(lastRemoved, Names({}));
    ASSERT_EQ(lastChangedGroups, Names({"Group 1", "Group 2"}));
    ASSERT_EQ(lastChangedMembers, Names({"Group 2", "User 1"}));
    ASSERT_EQ(directParents("Group 1"), Names({}));
    ASSERT_EQ(directMembers("Group 1"), Names({"Group 3"}));
    ASSERT_EQ(directParents("Group 2"), Names({"Group 4"}));
    ASSERT_EQ(directMembers("Group 2"), Names({"User 1"}));
    ASSERT_EQ(directParents("Group 3"), Names({"Group 1"}));
    ASSERT_EQ(directMembers("Group 3"), Names({"User 1", "User 2"}));
    ASSERT_EQ(directParents("Group 4"), Names({}));
    ASSERT_EQ(directMembers("Group 4"), Names({"Group 2"}));
    ASSERT_EQ(directParents("User 1"), Names({"Group 2", "Group 3"}));
    ASSERT_EQ(directMembers("User 1"), Names({}));
    ASSERT_EQ(directParents("User 2"), Names({"Group 3"}));
    ASSERT_EQ(directMembers("User 2"), Names({}));
}

} // namespace test
} // namespace nx::core::access
