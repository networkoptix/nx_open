// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <thread>

#include <gtest/gtest.h>

#include <nx/analytics/taxonomy/descriptor_container.h>
#include <nx/analytics/taxonomy/state_compiler.h>
#include <nx/vms/common/test_support/analytics/taxonomy/test_resource_support_proxy.h>
#include <nx/vms/common/test_support/analytics/taxonomy/utils.h>
#include <nx/vms/rules/aggregated_event.h>
#include <nx/vms/rules/event_filter_fields/analytics_object_type_field.h>
#include <nx/vms/rules/event_filter_fields/object_lookup_field.h>
#include <nx/vms/rules/events/analytics_object_event.h>
#include <nx/vms/rules/utils/event_parameter_helper.h>

#include "test_plugin.h"
#include "test_router.h"

namespace nx::vms::rules::test {

using namespace std::literals;

namespace {

constexpr auto kSimpleAttrs = R"json(
{
    "descriptors": {
        "objectTypeDescriptors": {
            "objectType1": {
                "id": "objectType1",
                "attributes": [
                    { "name": "Name", "type": "String" },
                    { "name": "Int", "type": "Number", "subtype": "integer" },
                    { "name": "LicensePlate", "type": "Object", "subtype": "LicensePlate" }

                ]
            },
            "LicensePlate": {
                "id": "LicensePlate",
                "attributes": [
                    { "name": "Number", "type": "String" },
                    { "name": "Country", "type": "String" }
                ]
            }
        }
    },
    "tests": {}
}
)json"sv;

const FieldDescriptor kDummyDescriptor;

} //namespace

class EventParameterTest: public EngineBasedTest, public TestPlugin
{
    using base_type = EngineBasedTest;

public:
    EventParameterTest(): TestPlugin(engine.get()) {}

    virtual void SetUp() override
    {
        base_type::SetUp();

        loadTaxonomy();

        ASSERT_TRUE(registerEventField<AnalyticsObjectTypeField>(systemContext()));
        ASSERT_TRUE(registerEventField<ObjectLookupField>(systemContext()));
        ASSERT_TRUE(registerEvent<AnalyticsObjectEvent>());
    }

    void loadTaxonomy()
    {
        nx::analytics::taxonomy::TestData testData;
        ASSERT_TRUE(nx::analytics::taxonomy::makeDescriptorsTestData(kSimpleAttrs, &testData));

        auto compiled = nx::analytics::taxonomy::StateCompiler::compile(testData.descriptors,
            std::make_unique<nx::analytics::taxonomy::TestResourceSupportProxy>());
        ASSERT_TRUE(compiled.errors.empty());

        auto descriptors = compiled.state->serialize();
        ASSERT_FALSE(descriptors.isEmpty());

        systemContext()->analyticsDescriptorContainer()
            ->updateDescriptorsForTests(std::move(descriptors));
    }

    QString evaluate(const EventPtr& event, const QString& param) const
    {
        return rules::utils::EventParameterHelper::instance()->evaluateEventParameter(
            systemContext(), AggregatedEventPtr::create(event), param);
    }

    QSharedPointer<AnalyticsObjectEvent> analyticsObject() const
    {
        auto event = QSharedPointer<AnalyticsObjectEvent>::create();
        event->setTimestamp(qnSyncTime->currentTimePoint());
        event->setObjectTypeId("objectType1");
        event->setObjectTrackId(nx::Uuid::createUuid());

        return event;
    }

    TestEventPtr testEvent() const
    {
        auto event = TestEventPtr::create();
        event->m_text = "test";
        event->m_cameraId = nx::Uuid::createUuid();
        event->m_deviceIds = {nx::Uuid::createUuid(), nx::Uuid::createUuid()};
        event->m_intField = 42;
        event->level = nx::vms::event::Level::success;

        return event;
    }
};

// FIXME: There are more parameter tests in ActionFieldTest. Consider moving them here.

TEST_F(EventParameterTest, simpleAttributes)
{
    auto event = analyticsObject();
    event->setAttributes({{"Name", "test"}, {"Int", "42"}});

    EXPECT_EQ("test", evaluate(event, "event.attributes.Name"));
    EXPECT_EQ("42", evaluate(event, "event.attributes.Int"));
    EXPECT_EQ("{event.attributes.undefined}", evaluate(event, "event.attributes.undefined"));

    event->setAttributes({});
    // TODO: #vbutkevich. Change to "" instead "{event.attributes.Name}". An empty string should be
    // returned if the attribute is supported by the objectTypeId but is not present in the event's
    // attributes.
    EXPECT_EQ("{event.attributes.Name}", evaluate(event, "event.attributes.Name"));
}

TEST_F(EventParameterTest, nestedAttributes)
{
    const QString kAttributeNameField = QStringLiteral("Name");
    const QString kAttributeName = QStringLiteral("Temporal Displacement");
    const QString kAttributeLicensePlateNumberField = QStringLiteral("LicensePlate.Number");
    const QString kAttributeLicensePlateNumber = QStringLiteral("OUTATIME");

    static const nx::common::metadata::Attributes kAttributes{
        {kAttributeNameField, kAttributeName},
        {kAttributeLicensePlateNumberField, kAttributeLicensePlateNumber}};


    auto event = analyticsObject();
    event->setAttributes(kAttributes);

    TextWithFields field(systemContext(), &kDummyDescriptor);
    field.setText(NX_FMT(
        "{event.attributes.%1} {event.attributes.%2} \"{event.attributes.none}\"",
        kAttributeNameField, kAttributeLicensePlateNumberField));

    const auto& actual = field.build(AggregatedEventPtr::create(event)).toString();
    const QString expected =
        NX_FMT("%1 %2 \"{event.attributes.none}\"", kAttributeName, kAttributeLicensePlateNumber);

    EXPECT_EQ(expected, actual);
}

TEST_F(EventParameterTest, fields)
{
    auto event = testEvent();

    EXPECT_EQ(event->m_text, evaluate(event, "event.fields.text"));
    EXPECT_EQ(QString::number(event->m_intField), evaluate(event, "event.fields.intField"));
    EXPECT_EQ(event->m_cameraId.toSimpleString(), evaluate(event, "event.fields.cameraId"));

    QStringList strings;
    for (auto id: event->m_deviceIds)
        strings.push_back(id.toSimpleString());

    EXPECT_EQ(strings.join(", "), evaluate(event, "event.fields.deviceIds"));

    // The enums are converted as integers.
    EXPECT_EQ(QString::number(int(event->level)), evaluate(event, "event.fields.level"));

    // The absent field should display as substitution.
    EXPECT_EQ("{event.fields.absent}", evaluate(event, "event.fields.absent"));

    // The field that has no conversion to string should display as quoted substitution.
    EXPECT_EQ("{event.fields.conflicts}", evaluate(event, "event.fields.conflicts"));
}

TEST_F(EventParameterTest, details)
{
    auto event = testEvent();
    const auto details = event->details(systemContext(), {});

    EXPECT_EQ(details.value("url").toString(), evaluate(event, "event.details.url"));
    EXPECT_EQ(details.value("number").toString(), evaluate(event, "event.details.number"));
    EXPECT_EQ(event->m_cameraId.toSimpleString(), evaluate(event, "event.details.sourceId"));

    EXPECT_EQ(
        details.value("detailing").value<QStringList>().join(", "),
        evaluate(event, "event.details.detailing"));

    // The enums are converted as integers.
    EXPECT_EQ(QString::number(int(event->level)), evaluate(event, "event.details.level"));

    // The absent field should display as substitution.
    EXPECT_EQ("{event.details.absent}", evaluate(event, "event.details.absent"));

    // The field that has no conversion to string should display as quoted substitution.
    EXPECT_EQ("{event.details.stdString}", evaluate(event, "event.details.stdString"));
}

} // nx::vms::rules::test
