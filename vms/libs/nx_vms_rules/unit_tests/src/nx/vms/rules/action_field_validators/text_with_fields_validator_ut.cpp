// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/vms/common/system_context.h>
#include <nx/vms/rules/action_builder.h>
#include <nx/vms/rules/action_builder_field_validators/text_with_fields_validator.h>
#include <nx/vms/rules/action_builder_fields/text_with_fields.h>
#include <nx/vms/rules/event_filter.h>
#include <nx/vms/rules/manifest.h>
#include <nx/vms/rules/rule.h>

#include "../test_event.h"
#include "../test_plugin.h"
#include "../test_router.h"

namespace nx::vms::rules::test {

class TestActionWithUrl: public nx::vms::rules::BasicAction
{
    Q_OBJECT
    Q_CLASSINFO("type", "nx.actions.test.withUrl")

    Q_PROPERTY(QString text MEMBER m_text)

public:
    static ItemDescriptor manifest()
    {
        return ItemDescriptor{
            .id = utils::type<TestActionWithUrl>(),
            .displayName = TranslatableString("Test action with url"),
            .fields = {
                makeFieldDescriptor<TextWithFields>(
                    "text",
                    TranslatableString("Text with fields"),
                    {},
                    TextWithFieldsFieldProperties{
                        .validationPolicy = kUrlValidationPolicy}.toVariantMap())
            }
        };
    }

    QString m_text;
};

class TextWithFieldsValidatorTest: public EngineBasedTest, public TestPlugin
{
public:
    TextWithFieldsValidatorTest(): TestPlugin{engine.get()}
    {
        registerActionField<TextWithFields>(m_engine->systemContext());
        registerAction<TestActionWithUrl>();

        m_rule = makeRule<TestEventInstant, TestActionWithUrl>();
    }

    void whenTextIs(const QString& text)
    {
        textWithFields()->setText(text);
    }

    void thanFieldIsInvalid()
    {
        EXPECT_EQ(validity(), QValidator::State::Invalid);
    }

    void thanFieldIsIntermediate()
    {
        EXPECT_EQ(validity(), QValidator::State::Intermediate);
    }

    void thanFieldIsAccepted()
    {
        EXPECT_EQ(validity(), QValidator::State::Acceptable);
    }

private:
    TextWithFields* textWithFields() const
    {
        return m_rule->actionBuilders().first()->fieldByType<TextWithFields>();
    }

    QValidator::State validity() const
    {
        return m_textWithFieldsValidator.validity(
            textWithFields(),
            m_rule.get(),
            systemContext()).validity;
    }

    TextWithFieldsValidator m_textWithFieldsValidator;
    std::unique_ptr<Rule> m_rule;
};

TEST_F(TextWithFieldsValidatorTest, emptyUrlIsInvalid)
{
    whenTextIs("");
    thanFieldIsInvalid();
}

TEST_F(TextWithFieldsValidatorTest, validUrlIsAccepted)
{
    whenTextIs("http://foo.bar");
    thanFieldIsAccepted();

    whenTextIs("  http://foo.bar      ");
    thanFieldIsAccepted();

    whenTextIs("https://www.example.org:443/path/page.html"
        "?query=param1&query2=param2+param3;query3=sp%0Ace#segment");
    thanFieldIsAccepted();
}

TEST_F(TextWithFieldsValidatorTest, urlShouldNotContainsCredentials)
{
    // It is a deprecated method of passing credentials. Actions (currently, only HTTP action),
    // which require a user/password have special fields for them.
    whenTextIs("http://login:password@foo.bar");
    thanFieldIsIntermediate();

    whenTextIs("https://user:password@www.example.org:443/path/page.html"
        "?query=param1&query2=param2+param3;query3=sp%0Ace#segment");
    thanFieldIsIntermediate();
}

TEST_F(TextWithFieldsValidatorTest, urlWithSubstitutionIsValid)
{
    whenTextIs("http://foo.bar{foo.bar}");
    thanFieldIsAccepted();

    whenTextIs("http://foo.bar{substitution.with space}");
    thanFieldIsAccepted();

    whenTextIs("http://foo.bar?q={foo.bar}");
    thanFieldIsAccepted();

    whenTextIs("http:://12345/{foo.bar}/12345{baz.bat}");
    thanFieldIsAccepted();
}

TEST_F(TextWithFieldsValidatorTest, onlySubstitutionIsAccepted)
{
    whenTextIs("{foo.bar}");
    thanFieldIsAccepted();
}

} // namespace nx::vms::rules::test

#include "text_with_fields_validator_ut.moc"
