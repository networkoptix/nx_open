// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <memory>

#include <gtest/gtest.h>

#include <nx/vms/client/desktop/rules/model_view/rules_table_model.h>
#include <nx/vms/rules/basic_action.h>
#include <nx/vms/rules/basic_event.h>
#include <nx/vms/rules/engine.h>
#include <nx/vms/rules/router.h>

namespace nx::vms::client::desktop::rules::test {

class FakeRouter: public vms::rules::Router
{
public:
    void routeEvent(
        const vms::rules::EventData& /*eventData*/,
        const QSet<QnUuid>& /*triggeredRules*/,
        const QSet<QnUuid>& /*affectedResources*/) override
    {
    }
};

class FakeEvent: public vms::rules::BasicEvent
{
public:
    QString resourceKey() const override
    {
        return {};
    }

    static vms::rules::ItemDescriptor manifest()
    {
        return vms::rules::ItemDescriptor{
            .id = "fake.event",
            .displayName = "Fake event",
        };
    }
};

class FakeAction: public vms::rules::BasicAction
{
public:
    static vms::rules::ItemDescriptor manifest()
    {
        return vms::rules::ItemDescriptor{
            .id = "fake.action",
            .displayName = "Fake action",
        };
    }
};

class RulesTableModelTest: public ::testing::Test
{
protected:
    void SetUp() override
    {
        m_engine = std::make_unique<vms::rules::Engine>(std::make_unique<FakeRouter>());
        m_engine->registerEvent(FakeEvent::manifest(), [] { return new FakeEvent; });
        m_engine->registerAction(FakeAction::manifest(), [] { return new FakeAction; });

        m_model = std::make_unique<RulesTableModel>(m_engine.get());
    }

    void TearDown() override
    {
        m_model.reset();
        m_engine.reset();
    }

    std::unique_ptr<vms::rules::Engine> m_engine;
    std::unique_ptr<RulesTableModel> m_model;
};

TEST_F(RulesTableModelTest, NotInitializedModelIsEmpty)
{
    // By default engine has no rules.
    ASSERT_FALSE(m_engine->hasRules());
    // Therefore model has no records.
    ASSERT_EQ(m_model->rowCount(), 0);
    // And has no changes.
    ASSERT_FALSE(m_model->hasChanges());
}

TEST_F(RulesTableModelTest, RuleAddedSuccessfully)
{
    // When new rule is added successfully.
    const auto newRuleIndex = m_model->addRule(FakeEvent::manifest().id, FakeAction::manifest().id);
    ASSERT_TRUE(newRuleIndex.isValid());

    // Model has one record.
    ASSERT_EQ(m_model->rowCount(), 1);
    // And hasChanges flag is set.
    ASSERT_TRUE(m_model->hasChanges());

    // Added rule is accessed by the index.
    auto simplifiedRuleWeak = m_model->rule(newRuleIndex);
    ASSERT_FALSE(simplifiedRuleWeak.expired());
    // And stored index is the same as index got on the addRule step.
    auto simplifiedRule = simplifiedRuleWeak.lock();
    ASSERT_EQ(simplifiedRule->modelIndex(), newRuleIndex);
}

TEST_F(RulesTableModelTest, ModelChangesRejectedSuccessfully)
{
    // After model is changed.
    m_model->addRule(FakeEvent::manifest().id, FakeAction::manifest().id);

    // Reject changes method.
    m_model->rejectChanges();

    // Returns model to the initial state.
    ASSERT_FALSE(m_model->hasChanges());
    ASSERT_EQ(m_model->rowCount(), 0);
}

TEST_F(RulesTableModelTest, RuleAddedAndRemovedSuccessfully)
{
    // Added rule.
    const auto newRuleIndex = m_model->addRule(FakeEvent::manifest().id, FakeAction::manifest().id);
    // Removed successfully.
    ASSERT_TRUE(m_model->removeRule(newRuleIndex));
    ASSERT_EQ(m_model->rowCount(), 0);
    ASSERT_FALSE(m_model->hasChanges());
}

} // namespace nx::vms::client::desktop::rules::test
