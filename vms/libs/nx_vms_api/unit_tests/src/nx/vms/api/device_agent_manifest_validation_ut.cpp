// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <QtCore/QJsonArray>

#include <nx/vms/api/base_manifest_validation_test.h>
#include <nx/vms/api/analytics/device_agent_manifest.h>

namespace nx::vms::api::analytics {

static const qsizetype kNumberOfEntriesToTransform = 3;

class DeviceAgentManifestValidationTest: public BaseManifestValidationTest<DeviceAgentManifest>
{
protected:
    void givenCorrectManifest()
    {
        m_manifest = DeviceAgentManifest();
        m_manifest.supportedEventTypeIds = makeStrings("supportedEventType");
        m_manifest.supportedObjectTypeIds = makeStrings("supportedObjectType");
        m_manifest.eventTypes = makeEntries<AnalyticsEventType>("Event");
        m_manifest.objectTypes = makeEntries<ObjectType>("Object");
        m_manifest.groups = makeEntries<Group>("Group");
        m_manifest.deviceAgentSettingsModel = QJsonObject();
    }

    template<typename List, typename TransformationFunc>
    void makeIncorrectManifest(List* inOutList, TransformationFunc transformationFunc)
    {
        givenCorrectManifest();
        transformSomeEntries(inOutList, kNumberOfEntriesToTransform, transformationFunc);
    }

    //---------------------------------------------------------------------------------------------

    void givenManifestWithEmptyEventTypeIds()
    {
        makeIncorrectManifest(&m_manifest.eventTypes, [](auto entry) { entry->id.clear(); });
    }

    void givenManifestWithDuplicatedEventTypeIds()
    {
        makeIncorrectManifest(&m_manifest.eventTypes, [](auto entry) { entry->id = "id"; });
    }

    void givenManifestWithEmptyEventTypeNames()
    {
        makeIncorrectManifest(&m_manifest.eventTypes, [](auto entry) { entry->name.clear(); });
    }

    void givenManifestWithDuplicatedEventTypeNames()
    {
        makeIncorrectManifest(&m_manifest.eventTypes, [](auto entry) { entry->name = "name"; });
    }

    void givenManifestWithDuplicatedEventTypeIdsInSupportedList()
    {
        givenCorrectManifest();
        m_manifest.supportedEventTypeIds.push_back("id");
        m_manifest.supportedEventTypeIds.push_back("id");
    }

    void givenManifestWithDuplicatedEventTypeIdsInBothLists()
    {
        givenCorrectManifest();
        m_manifest.supportedEventTypeIds.push_back("id");

        AnalyticsEventType eventType;
        eventType.id = "id";
        eventType.name = "some name";
        m_manifest.eventTypes.push_back(eventType);
    }

    //---------------------------------------------------------------------------------------------

    void givenManifestWithEmptyObjectTypeIds()
    {
        makeIncorrectManifest(&m_manifest.objectTypes, [](auto entry) { entry->id.clear(); });
    }

    void givenManifestWithDuplicatedObjectTypeIds()
    {
        makeIncorrectManifest(&m_manifest.objectTypes, [](auto entry) { entry->id = "name"; });
    }

    void givenManifestWithDuplicatedObjectTypeNames()
    {
        makeIncorrectManifest(&m_manifest.objectTypes, [](auto entry) { entry->name = "name"; });
    }

    void givenManifestWithDuplicatedObjectTypeIdsInSupportedList()
    {
        givenCorrectManifest();
        m_manifest.supportedObjectTypeIds.push_back("id");
        m_manifest.supportedObjectTypeIds.push_back("id");
    }

    void givenManifestWithDuplicatedObjectTypeIdsInBothLists()
    {
        givenCorrectManifest();
        m_manifest.supportedObjectTypeIds.push_back("id");

        ObjectType objectType;
        objectType.id = "id";
        objectType.name = "some name";
        m_manifest.objectTypes.push_back(objectType);
    }

    //---------------------------------------------------------------------------------------------

    void givenManifestWithEmptyGroupIds()
    {
        makeIncorrectManifest(&m_manifest.groups, [](auto entry) { entry->id.clear(); });
    }

    void givenManifestWithDuplicatedGroupIds()
    {
        makeIncorrectManifest(&m_manifest.groups, [](auto entry) { entry->id = "name"; });
    }

    void givenManifestWithEmptyGroupNames()
    {
        makeIncorrectManifest(&m_manifest.groups, [](auto entry) { entry->name.clear(); });
    }

    void givenManifestWithDuplicatedGroupNames()
    {
        makeIncorrectManifest(&m_manifest.groups, [](auto entry) { entry->name = "name"; });
    }

    // --------------------------------------------------------------------------------------------

    template<typename Type>
    void givenManifestWithIncorrectTypeInSettingsModel()
    {
        givenCorrectManifest();
        m_manifest.deviceAgentSettingsModel = Type{};
    }
};

TEST_F(DeviceAgentManifestValidationTest, correctManifestProducesNoErrors)
{
    givenCorrectManifest();
    whenValidatingManifest();
    makeSureManifestIsCorrect();
}

TEST_F(DeviceAgentManifestValidationTest, manifestWithEmptyEventTypeIdsProducesAnError)
{
    givenManifestWithEmptyEventTypeIds();
    whenValidatingManifest();
    makeSureErrorsAreCaught({ManifestErrorType::emptyEventTypeId});
}

TEST_F(DeviceAgentManifestValidationTest, manifestWithDuplicatedEventTypeIdsProducesAnError)
{
    givenManifestWithDuplicatedEventTypeIds();
    whenValidatingManifest();
    makeSureErrorsAreCaught({ManifestErrorType::duplicatedEventTypeId});
}

TEST_F(DeviceAgentManifestValidationTest, manifestWithEmptyEventTypeNamesProducesAnError)
{
    givenManifestWithEmptyEventTypeNames();
    whenValidatingManifest();
    makeSureErrorsAreCaught({ManifestErrorType::emptyEventTypeName});
}

TEST_F(DeviceAgentManifestValidationTest, manifestWithDuplicatedEventTypeNamesProducesAnError)
{
    givenManifestWithDuplicatedEventTypeNames();
    whenValidatingManifest();
    makeSureErrorsAreCaught({ManifestErrorType::duplicatedEventTypeName});
}

TEST_F(DeviceAgentManifestValidationTest,
    manifestWithDuplicatedEventTypeIdsInSupportedListProducesAnError)
{
    givenManifestWithDuplicatedEventTypeIdsInSupportedList();
    whenValidatingManifest();
    makeSureErrorsAreCaught({ManifestErrorType::duplicatedEventTypeId});
}

TEST_F(DeviceAgentManifestValidationTest,
    manifestWithDuplicatedEventTypeIdsInBothListsProducesAnError)
{
    givenManifestWithDuplicatedEventTypeIdsInBothLists();
    whenValidatingManifest();
    makeSureErrorsAreCaught({ManifestErrorType::duplicatedEventTypeId});
}

//-------------------------------------------------------------------------------------------------

TEST_F(DeviceAgentManifestValidationTest, manifestWithEmptyObjectTypeIdsProducesAnError)
{
    givenManifestWithEmptyObjectTypeIds();
    whenValidatingManifest();
    makeSureErrorsAreCaught({ManifestErrorType::emptyObjectTypeId});
}

TEST_F(DeviceAgentManifestValidationTest, manifestWithDuplicatedObjectTypeIdsProducesAnError)
{
    givenManifestWithDuplicatedObjectTypeIds();
    whenValidatingManifest();
    makeSureErrorsAreCaught({ManifestErrorType::duplicatedObjectTypeId});
}

TEST_F(DeviceAgentManifestValidationTest, manifestWithDuplicatedObjectTypeNamesProducesAnError)
{
    givenManifestWithDuplicatedObjectTypeNames();
    whenValidatingManifest();
    makeSureErrorsAreCaught({ManifestErrorType::duplicatedObjectTypeName});
}

TEST_F(DeviceAgentManifestValidationTest,
    manifestWithDuplicatedObjectTypeIdsInSupportedListProducesAnError)
{
    givenManifestWithDuplicatedObjectTypeIdsInSupportedList();
    whenValidatingManifest();
    makeSureErrorsAreCaught({ManifestErrorType::duplicatedObjectTypeId});
}

TEST_F(DeviceAgentManifestValidationTest,
    manifestWithDuplicatedObjectTypeIdsInBothListsProducesAnError)
{
    givenManifestWithDuplicatedObjectTypeIdsInBothLists();
    whenValidatingManifest();
    makeSureErrorsAreCaught({ManifestErrorType::duplicatedObjectTypeId});
}

//-------------------------------------------------------------------------------------------------

TEST_F(DeviceAgentManifestValidationTest, manifestWithEmptyGroupIdsProducesAnError)
{
    givenManifestWithEmptyGroupIds();
    whenValidatingManifest();
    makeSureErrorsAreCaught({ManifestErrorType::emptyGroupId});
}

TEST_F(DeviceAgentManifestValidationTest, manifestWithDuplicatedGroupIdsProducesAnError)
{
    givenManifestWithDuplicatedGroupIds();
    whenValidatingManifest();
    makeSureErrorsAreCaught({ManifestErrorType::duplicatedGroupId});
}

TEST_F(DeviceAgentManifestValidationTest, manifestWithEmptyGroupNamesProducesAnError)
{
    givenManifestWithEmptyGroupNames();
    whenValidatingManifest();
    makeSureErrorsAreCaught({ManifestErrorType::emptyGroupName});
}

TEST_F(DeviceAgentManifestValidationTest, manifestWithDuplicatedGroupNamesProducesAnError)
{
    givenManifestWithDuplicatedGroupNames();
    whenValidatingManifest();
    makeSureErrorsAreCaught({ManifestErrorType::duplicatedGroupName});
}

TEST_F(DeviceAgentManifestValidationTest, manifestWithIncorrectTypeInSettingsModelProducesAnError)
{
    givenManifestWithIncorrectTypeInSettingsModel<QString>();
    whenValidatingManifest();
    makeSureErrorsAreCaught({ManifestErrorType::deviceAgentSettingsModelIsIncorrect});

    givenManifestWithIncorrectTypeInSettingsModel<int>();
    whenValidatingManifest();
    makeSureErrorsAreCaught({ManifestErrorType::deviceAgentSettingsModelIsIncorrect});

    givenManifestWithIncorrectTypeInSettingsModel<QJsonArray>();
    whenValidatingManifest();
    makeSureErrorsAreCaught({ManifestErrorType::deviceAgentSettingsModelIsIncorrect});
}

} // namespace nx::vms::api::analytics
