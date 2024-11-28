// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/vms/api/base_manifest_validation_test.h>
#include <nx/vms/api/analytics/engine_manifest.h>
#include <nx/vms/api/analytics/object_action.h>

namespace nx::vms::api::analytics {

static const qsizetype kNumberOfEntriesToTransform = 3;

class EngineManifestValidationTest: public BaseManifestValidationTest<EngineManifest>
{
protected:
    void givenCorrectManifest()
    {
        m_manifest = EngineManifest();
        m_manifest.eventTypes = makeEntries<AnalyticsEventType>("Event");
        m_manifest.objectTypes = makeEntries<ObjectType>("Object");
        m_manifest.groups = makeEntries<Group>("Group");
        m_manifest.objectActions = makeEntries<ObjectAction>("ObjectAction");
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

    //---------------------------------------------------------------------------------------------

    void givenManifestWithEmptyObjectTypeIds()
    {
        makeIncorrectManifest(&m_manifest.objectTypes, [](auto entry) { entry->id.clear(); });
    }

    void givenManifestWithDuplicatedObjectTypeIds()
    {
        makeIncorrectManifest(&m_manifest.objectTypes, [](auto entry) { entry->id = "name"; });
    }

    void givenManifestWithEmptyObjectTypeNames()
    {
        makeIncorrectManifest(&m_manifest.objectTypes, [](auto entry) { entry->name.clear(); });
    }

    void givenManifestWithDuplicatedObjectTypeNames()
    {
        makeIncorrectManifest(&m_manifest.objectTypes, [](auto entry) { entry->name = "name"; });
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

    //---------------------------------------------------------------------------------------------

    void givenManifestWithEmptyObjectActionIds()
    {
        makeIncorrectManifest(&m_manifest.objectActions, [](auto entry) { entry->id.clear(); });
    }

    void givenManifestWithDuplicatedObjectActionIds()
    {
        makeIncorrectManifest(&m_manifest.objectActions, [](auto entry) { entry->id = "name"; });
    }

    void givenManifestWithEmptyObjectActionNames()
    {
        makeIncorrectManifest(&m_manifest.objectActions, [](auto entry) { entry->name.clear(); });
    }

    void givenManifestWithDuplicatedObjectActionNames()
    {
        makeIncorrectManifest(&m_manifest.objectActions, [](auto entry) { entry->name = "name"; });
    }

    //---------------------------------------------------------------------------------------------

    void givenManifestWithRequestUncompressedVideoStreamAndUnspecifiedPixelFormat()
    {
        givenCorrectManifest();
        m_manifest.capabilities = EngineCapabilities();
        m_manifest.streamTypeFilter = StreamType::uncompressedVideo;
    }

    void givenManifestWithoutRequestedUncompressedVideoStreamButWithSpecifiedPixelFormat()
    {
        givenCorrectManifest();
        m_manifest.capabilities = EngineCapability::needUncompressedVideoFrames_abgr;
        m_manifest.streamTypeFilter = StreamType::compressedVideo;
    }
};

TEST_F(EngineManifestValidationTest, correctManifestProducesNoErrors)
{
    givenCorrectManifest();
    whenValidatingManifest();
    makeSureManifestIsCorrect();
}

TEST_F(EngineManifestValidationTest, manifestWithEmptyEventTypeIdsProducesAnError)
{
    givenManifestWithEmptyEventTypeIds();
    whenValidatingManifest();
    makeSureErrorsAreCaught({ManifestErrorType::emptyEventTypeId});
}

TEST_F(EngineManifestValidationTest, manifestWithDuplicatedEventTypeIdsProducesAnError)
{
    givenManifestWithDuplicatedEventTypeIds();
    whenValidatingManifest();
    makeSureErrorsAreCaught({ManifestErrorType::duplicatedEventTypeId});
}

TEST_F(EngineManifestValidationTest, manifestWithEmptyEventTypeNamesProducesAnError)
{
    givenManifestWithEmptyEventTypeNames();
    whenValidatingManifest();
    makeSureErrorsAreCaught({ManifestErrorType::emptyEventTypeName});
}

TEST_F(EngineManifestValidationTest, manifestWithDuplicatedEventTypeNamesProducesAnError)
{
    givenManifestWithDuplicatedEventTypeNames();
    whenValidatingManifest();
    makeSureErrorsAreCaught({ManifestErrorType::duplicatedEventTypeName});
}

//-------------------------------------------------------------------------------------------------

TEST_F(EngineManifestValidationTest, manifestWithEmptyObjectTypeIdsProducesAnError)
{
    givenManifestWithEmptyObjectTypeIds();
    whenValidatingManifest();
    makeSureErrorsAreCaught({ManifestErrorType::emptyObjectTypeId});
}

TEST_F(EngineManifestValidationTest, manifestWithDuplicatedObjectTypeIdsProducesAnError)
{
    givenManifestWithDuplicatedObjectTypeIds();
    whenValidatingManifest();
    makeSureErrorsAreCaught({ManifestErrorType::duplicatedObjectTypeId});
}

TEST_F(EngineManifestValidationTest, manifestWithDuplicatedObjectTypeNamesProducesAnError)
{
    givenManifestWithDuplicatedObjectTypeNames();
    whenValidatingManifest();
    makeSureErrorsAreCaught({ManifestErrorType::duplicatedObjectTypeName});
}

//-------------------------------------------------------------------------------------------------

TEST_F(EngineManifestValidationTest, manifestWithEmptyGroupIdsProducesAnError)
{
    givenManifestWithEmptyGroupIds();
    whenValidatingManifest();
    makeSureErrorsAreCaught({ManifestErrorType::emptyGroupId});
}

TEST_F(EngineManifestValidationTest, manifestWithDuplicatedGroupIdsProducesAnError)
{
    givenManifestWithDuplicatedGroupIds();
    whenValidatingManifest();
    makeSureErrorsAreCaught({ManifestErrorType::duplicatedGroupId});
}

TEST_F(EngineManifestValidationTest, manifestWithEmptyGroupNamesProducesAnError)
{
    givenManifestWithEmptyGroupNames();
    whenValidatingManifest();
    makeSureErrorsAreCaught({ManifestErrorType::emptyGroupName});
}

TEST_F(EngineManifestValidationTest, manifestWithDuplicatedGroupNamesProducesAnError)
{
    givenManifestWithDuplicatedGroupNames();
    whenValidatingManifest();
    makeSureErrorsAreCaught({ManifestErrorType::duplicatedGroupName});
}

//-------------------------------------------------------------------------------------------------

TEST_F(EngineManifestValidationTest, manifestWithEmptyObjectActionIdsProducesAnError)
{
    givenManifestWithEmptyObjectActionIds();
    whenValidatingManifest();
    makeSureErrorsAreCaught({ManifestErrorType::emptyObjectActionId});
}

TEST_F(EngineManifestValidationTest, manifestWithDuplicatedObjectActionIdsProducesAnError)
{
    givenManifestWithDuplicatedObjectActionIds();
    whenValidatingManifest();
    makeSureErrorsAreCaught({ManifestErrorType::duplicatedObjectActionId});
}

TEST_F(EngineManifestValidationTest, manifestWithEmptyObjectActionNamesProducesAnError)
{
    givenManifestWithEmptyObjectActionNames();
    whenValidatingManifest();
    makeSureErrorsAreCaught({ManifestErrorType::emptyObjectActionName});
}

TEST_F(EngineManifestValidationTest, manifestWithDuplicatedObjectActionNamesProducesAnError)
{
    givenManifestWithDuplicatedObjectActionNames();
    whenValidatingManifest();
    makeSureErrorsAreCaught({ManifestErrorType::duplicatedObjectActionName});
}

//-------------------------------------------------------------------------------------------------

TEST_F(EngineManifestValidationTest, manifestWithMissingPixelFormat)
{
    givenManifestWithRequestUncompressedVideoStreamAndUnspecifiedPixelFormat();
    whenValidatingManifest();
    makeSureErrorsAreCaught({ManifestErrorType::uncompressedFramePixelFormatIsNotSpecified});
}

TEST_F(EngineManifestValidationTest, manifestWithExcessivePixelFormatSpecification)
{
    givenManifestWithoutRequestedUncompressedVideoStreamButWithSpecifiedPixelFormat();
    whenValidatingManifest();
    makeSureErrorsAreCaught(
        {ManifestErrorType::excessiveUncompressedFramePixelFormatSpecification});
}

} // namespace nx::vms::api::analytics
