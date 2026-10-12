// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <functional>
#include <memory>

#include <gtest/gtest.h>

#include <QtGui/QColor>

#include <core/resource_management/resource_pool.h>
#include <nx/vms/api/data/lookup_list_data.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/desktop/rules/model_view/rules_table_model.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/test_support/test_context.h>
#include <nx/vms/common/lookup_lists/lookup_list_manager.h>
#include <nx/vms/rules/action_builder.h>
#include <nx/vms/rules/actions/write_to_log_action.h>
#include <nx/vms/rules/engine.h>
#include <nx/vms/rules/event_filter.h>
#include <nx/vms/rules/event_filter_fields/source_camera_field.h>
#include <nx/vms/rules/event_filter_fields/text_lookup_field.h>
#include <nx/vms/rules/events/integration_diagnostic_event.h>
#include <nx/vms/rules/initializer.h>
#include <nx/vms/rules/router.h>
#include <nx/vms/rules/rule.h>
#include <nx/vms/rules/utils/api.h>
#include <nx/vms/rules/utils/field_names.h>
#include <nx/vms/rules/utils/rule.h>
#include <nx/vms/rules/utils/type.h>

namespace nx::vms::client::desktop::rules::test {

namespace {

using nx::vms::rules::Rule;

// Colors used by the model for the valid enabled rules and for all the other ones.
constexpr auto kEnabledForegroundColor = "light10";
constexpr auto kDisabledForegroundColor = "dark16";

} // namespace

class RulesTableModelTest: public desktop::test::ContextBasedTest
{
    using base_type = desktop::test::ContextBasedTest;

protected:
    virtual void SetUp() override
    {
        // There is no rules engine in the unit tests system context, so create one.
        m_engine = std::make_unique<vms::rules::Engine>(systemContext(), /*router*/ nullptr);
        vms::rules::Initializer initializer(systemContext());
        initializer.initialize(m_engine.get());

        m_lookupList.id = nx::Uuid::createUuid();
        m_lookupList.name = "Lookup list";
        systemContext()->lookupListManager()->initialize({m_lookupList});
    }

    virtual void TearDown() override
    {
        m_model.reset();
        base_type::TearDown();

        // The system context is shared between the tests.
        systemContext()->setVmsRulesEngine(nullptr);
        m_engine.reset();
        systemContext()->lookupListManager()->deinitialize();
    }

    void createModel() { m_model = std::make_unique<RulesTableModel>(systemContext()); }

    nx::Uuid addRule(const std::function<void(Rule*)>& setup = {})
    {
        auto rule = std::make_unique<Rule>(nx::Uuid::createUuid(), m_engine.get());
        rule->addEventFilter(m_engine->buildEventFilter(
            vms::rules::utils::type<vms::rules::IntegrationDiagnosticEvent>()));
        rule->addActionBuilder(
            m_engine->buildActionBuilder(vms::rules::utils::type<vms::rules::WriteToLogAction>()));
        rule->setEnabled(true);

        if (setup)
            setup(rule.get());

        m_engine->updateRule(vms::rules::serialize(rule.get()));
        return rule->id();
    }

    void modifyRule(nx::Uuid id, const std::function<void(Rule*)>& change)
    {
        auto rule = m_engine->cloneRule(id);
        change(rule.get());
        m_engine->updateRule(vms::rules::serialize(rule.get()));
    }

    static vms::rules::SourceCameraField* cameraField(Rule* rule)
    {
        return rule->eventFilters().first()->fieldByName<vms::rules::SourceCameraField>(
            vms::rules::utils::kDeviceIdFieldName);
    }

    // The rule is invalid while the camera is not in the resource pool.
    static void referenceCamera(Rule* rule, nx::Uuid cameraId)
    {
        cameraField(rule)->setAcceptAll(false);
        cameraField(rule)->setIds({cameraId});
    }

    static void referenceAnyCamera(Rule* rule)
    {
        cameraField(rule)->setAcceptAll(true);
        cameraField(rule)->setIds({});
    }

    // The rule is invalid while the lookup list does not exist.
    static void referenceList(Rule* rule, nx::Uuid listId)
    {
        const auto field = rule->eventFilters().first()->fieldByName<vms::rules::TextLookupField>(
            vms::rules::utils::kCaptionFieldName);
        field->setCheckType(vms::rules::TextLookupCheckType::inList);
        field->setValue(listId.toSimpleString());
    }

    QnResourcePtr addCamera(nx::Uuid id)
    {
        const auto camera = createCamera();
        camera->setIdUnsafe(id);
        resourcePool()->addResource(camera);
        return camera;
    }

    nx::vms::common::LookupListManager* lookupListManager()
    {
        return systemContext()->lookupListManager();
    }

    bool isRuleValid(nx::Uuid id)
    {
        const auto rule = m_engine->rule(id);
        return rule
            && vms::rules::utils::visibleFieldsValidity(rule.get(), systemContext()).validity
            != QValidator::State::Invalid;
    }

    void checkModelMatchesRules()
    {
        using core::colorTheme;

        ASSERT_EQ(m_model->rowCount({}), (int) m_engine->rules().size());

        for (int row = 0; row < m_model->rowCount({}); ++row)
        {
            const auto stateIndex = m_model->index(row, RulesTableModel::StateColumn);
            const auto rule = m_model->getRule(stateIndex);
            ASSERT_TRUE(rule);

            const bool isValid = isRuleValid(rule->id());
            const auto ruleId = rule->id().toSimpleStdString();

            // The attention icon is shown for the invalid rules only.
            EXPECT_EQ(m_model->data(stateIndex, Qt::DecorationRole).isNull(), isValid)
                << "Rule: " << ruleId;

            const auto expectedColor =
                colorTheme()->color(isValid ? kEnabledForegroundColor : kDisabledForegroundColor);

            for (int column = 0; column < m_model->columnCount({}); ++column)
            {
                const auto color =
                    m_model->data(m_model->index(row, column), Qt::ForegroundRole).value<QColor>();
                EXPECT_EQ(color, expectedColor) << "Rule: " << ruleId << ", column: " << column;
            }
        }
    }

protected:
    std::unique_ptr<vms::rules::Engine> m_engine;
    std::unique_ptr<RulesTableModel> m_model;
    nx::vms::api::LookupListData m_lookupList;
};

TEST_F(RulesTableModelTest, initialData)
{
    const auto missingCameraId = nx::Uuid::createUuid();
    const auto missingListId = nx::Uuid::createUuid();

    const auto plainRule = addRule();
    const auto cameraRule = addRule([&](Rule* rule) { referenceCamera(rule, missingCameraId); });
    const auto listRule = addRule([&](Rule* rule) { referenceList(rule, m_lookupList.id); });
    const auto missingListRule = addRule([&](Rule* rule) { referenceList(rule, missingListId); });

    createModel();

    ASSERT_TRUE(isRuleValid(plainRule));
    ASSERT_FALSE(isRuleValid(cameraRule));
    ASSERT_TRUE(isRuleValid(listRule));
    ASSERT_FALSE(isRuleValid(missingListRule));
    checkModelMatchesRules();
}

TEST_F(RulesTableModelTest, ruleUpdate)
{
    const auto missingCameraId = nx::Uuid::createUuid();

    const auto plainRule = addRule();
    const auto cameraRule = addRule([&](Rule* rule) { referenceCamera(rule, missingCameraId); });

    createModel();
    checkModelMatchesRules();

    modifyRule(plainRule, [&](Rule* rule) { referenceCamera(rule, missingCameraId); });
    modifyRule(cameraRule, [](Rule* rule) { referenceAnyCamera(rule); });

    ASSERT_FALSE(isRuleValid(plainRule));
    ASSERT_TRUE(isRuleValid(cameraRule));
    checkModelMatchesRules();
}

TEST_F(RulesTableModelTest, resourcePoolChanges)
{
    const auto cameraId = nx::Uuid::createUuid();

    const auto cameraRule = addRule([&](Rule* rule) { referenceCamera(rule, cameraId); });
    addRule();

    createModel();
    checkModelMatchesRules();

    const auto camera = addCamera(cameraId);
    ASSERT_TRUE(isRuleValid(cameraRule));
    checkModelMatchesRules();

    resourcePool()->removeResource(camera);
    ASSERT_FALSE(isRuleValid(cameraRule));
    checkModelMatchesRules();
}

TEST_F(RulesTableModelTest, lookupListRemoval)
{
    const auto listRule1 = addRule([&](Rule* rule) { referenceList(rule, m_lookupList.id); });
    const auto listRule2 = addRule([&](Rule* rule) { referenceList(rule, m_lookupList.id); });
    addRule();

    createModel();
    checkModelMatchesRules();

    lookupListManager()->remove(m_lookupList.id);

    ASSERT_FALSE(isRuleValid(listRule1));
    ASSERT_FALSE(isRuleValid(listRule2));
    checkModelMatchesRules();
}

TEST_F(RulesTableModelTest, rulesReset)
{
    const auto missingCameraId = nx::Uuid::createUuid();

    const auto plainRule = addRule();
    const auto cameraRule = addRule([&](Rule* rule) { referenceCamera(rule, missingCameraId); });

    createModel();
    checkModelMatchesRules();

    const auto changedPlainRule = m_engine->cloneRule(plainRule);
    referenceCamera(changedPlainRule.get(), missingCameraId);

    const auto changedCameraRule = m_engine->cloneRule(cameraRule);
    referenceAnyCamera(changedCameraRule.get());

    m_engine->resetRules({vms::rules::serialize(changedPlainRule.get()),
        vms::rules::serialize(changedCameraRule.get())});

    ASSERT_FALSE(isRuleValid(plainRule));
    ASSERT_TRUE(isRuleValid(cameraRule));
    checkModelMatchesRules();
}

TEST_F(RulesTableModelTest, reopenAfterChangesWhileClosed)
{
    const auto cameraId = nx::Uuid::createUuid();

    nx::vms::api::LookupListData newList;
    newList.id = nx::Uuid::createUuid();
    newList.name = "New lookup list";

    const auto cameraRule = addRule([&](Rule* rule) { referenceCamera(rule, cameraId); });
    const auto listRule = addRule([&](Rule* rule) { referenceList(rule, m_lookupList.id); });
    const auto newListRule = addRule([&](Rule* rule) { referenceList(rule, newList.id); });

    createModel();
    checkModelMatchesRules();

    m_model->setActive(false);

    addCamera(cameraId);
    lookupListManager()->remove(m_lookupList.id);
    lookupListManager()->addOrUpdate(newList); //< Not tracked by the model at all.
    addRule();

    m_model->setActive(true);

    ASSERT_TRUE(isRuleValid(cameraRule));
    ASSERT_FALSE(isRuleValid(listRule));
    ASSERT_TRUE(isRuleValid(newListRule));
    checkModelMatchesRules();
}

} // namespace nx::vms::client::desktop::rules::test
