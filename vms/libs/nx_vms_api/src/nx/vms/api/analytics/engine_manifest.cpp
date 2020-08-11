#include "engine_manifest.h"

#include <set>

#include <nx/fusion/model_functions.h>

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::vms::api::analytics::EngineManifest, Capability,
    (nx::vms::api::analytics::EngineManifest::Capability::noCapabilities, "noCapabilities")
    (nx::vms::api::analytics::EngineManifest::Capability::needUncompressedVideoFrames_yuv420, "needUncompressedVideoFrames_yuv420")
    (nx::vms::api::analytics::EngineManifest::Capability::needUncompressedVideoFrames_argb, "needUncompressedVideoFrames_argb")
    (nx::vms::api::analytics::EngineManifest::Capability::needUncompressedVideoFrames_abgr, "needUncompressedVideoFrames_abgr")
    (nx::vms::api::analytics::EngineManifest::Capability::needUncompressedVideoFrames_rgba, "needUncompressedVideoFrames_rgba")
    (nx::vms::api::analytics::EngineManifest::Capability::needUncompressedVideoFrames_bgra, "needUncompressedVideoFrames_bgra")
    (nx::vms::api::analytics::EngineManifest::Capability::needUncompressedVideoFrames_rgb, "needUncompressedVideoFrames_rgb")
    (nx::vms::api::analytics::EngineManifest::Capability::needUncompressedVideoFrames_bgr, "needUncompressedVideoFrames_bgr")
    (nx::vms::api::analytics::EngineManifest::Capability::deviceDependent, "deviceDependent")
    (nx::vms::api::analytics::EngineManifest::Capability::keepObjectBoundingBoxRotation, "keepObjectBoundingBoxRotation")
    (nx::vms::api::analytics::EngineManifest::Capability::noAutoBestShots, "noAutoBestShots")
)
QN_FUSION_DEFINE_FUNCTIONS(nx::vms::api::analytics::EngineManifest::Capability, (numeric)(debug))

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::vms::api::analytics::EngineManifest, Capabilities,
    (nx::vms::api::analytics::EngineManifest::Capability::noCapabilities, "noCapabilities")
    (nx::vms::api::analytics::EngineManifest::Capability::needUncompressedVideoFrames_yuv420, "needUncompressedVideoFrames_yuv420")
    (nx::vms::api::analytics::EngineManifest::Capability::needUncompressedVideoFrames_argb, "needUncompressedVideoFrames_argb")
    (nx::vms::api::analytics::EngineManifest::Capability::needUncompressedVideoFrames_abgr, "needUncompressedVideoFrames_abgr")
    (nx::vms::api::analytics::EngineManifest::Capability::needUncompressedVideoFrames_rgba, "needUncompressedVideoFrames_rgba")
    (nx::vms::api::analytics::EngineManifest::Capability::needUncompressedVideoFrames_bgra, "needUncompressedVideoFrames_bgra")
    (nx::vms::api::analytics::EngineManifest::Capability::needUncompressedVideoFrames_rgb, "needUncompressedVideoFrames_rgb")
    (nx::vms::api::analytics::EngineManifest::Capability::needUncompressedVideoFrames_bgr, "needUncompressedVideoFrames_bgr")
    (nx::vms::api::analytics::EngineManifest::Capability::deviceDependent, "deviceDependent")
    (nx::vms::api::analytics::EngineManifest::Capability::keepObjectBoundingBoxRotation, "keepObjectBoundingBoxRotation")
    (nx::vms::api::analytics::EngineManifest::Capability::noAutoBestShots, "noAutoBestShots")
)
QN_FUSION_DEFINE_FUNCTIONS(nx::vms::api::analytics::EngineManifest::Capabilities, (numeric)(debug))

//-------------------------------------------------------------------------------------------------

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(
    nx::vms::api::analytics::EngineManifest::ObjectAction, Capability,
    (nx::vms::api::analytics::EngineManifest::ObjectAction::Capability::noCapabilities,
        "noCapabilities")
    (nx::vms::api::analytics::EngineManifest::ObjectAction::Capability::needBestShotVideoFrame,
        "needBestShotVideoFrame")
    (nx::vms::api::analytics::EngineManifest::ObjectAction::Capability::needBestShotObjectMetadata,
        "needBestShotObjectMetadata")
    (nx::vms::api::analytics::EngineManifest::ObjectAction::Capability::needFullTrack, "needFullTrack")
    (nx::vms::api::analytics::EngineManifest::ObjectAction::Capability::needBestShotImage, "needBestShotImage")
)
QN_FUSION_DEFINE_FUNCTIONS(
    nx::vms::api::analytics::EngineManifest::ObjectAction::Capability, (numeric)(debug))

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(
    nx::vms::api::analytics::EngineManifest::ObjectAction, Capabilities,
    (nx::vms::api::analytics::EngineManifest::ObjectAction::Capability::noCapabilities,
        "noCapabilities")
    (nx::vms::api::analytics::EngineManifest::ObjectAction::Capability::needBestShotVideoFrame,
        "needBestShotVideoFrame")
    (nx::vms::api::analytics::EngineManifest::ObjectAction::Capability::needBestShotObjectMetadata,
        "needBestShotObjectMetadata")
    (nx::vms::api::analytics::EngineManifest::ObjectAction::Capability::needFullTrack, "needFullTrack")
    (nx::vms::api::analytics::EngineManifest::ObjectAction::Capability::needBestShotImage, "needBestShotImage")
)
QN_FUSION_DEFINE_FUNCTIONS(
    nx::vms::api::analytics::EngineManifest::ObjectAction::Capabilities, (numeric)(debug))

//-------------------------------------------------------------------------------------------------

namespace nx::vms::api::analytics {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(EngineManifest::ObjectAction::Requirements, (json)(eq),
    nx_vms_api_analytics_Engine_ObjectAction_Requirements_Fields, (brief, true))

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(EngineManifest::ObjectAction, (json),
    ObjectAction_Fields, (brief, true))

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(EngineManifest, (json),
    EngineManifest_Fields, (brief, true))

template<typename List, typename FieldValueGetter>
void validateListByField(
    std::vector<ManifestError>* outErrorList,
    const List& list,
    const EntryFieldManifestErrorTypes& entryFieldErrorTypes,
    FieldValueGetter fieldValueGetter)
{
    if (!NX_ASSERT(outErrorList))
        return;

    bool isEmptyFieldDetected = false;
    std::set<QString> processedFields;
    std::set<QString> processedFieldDuplicates;

    for (const auto& entry: list)
    {
        const auto fieldValue = fieldValueGetter(entry);
        if (fieldValue.isEmpty() && !isEmptyFieldDetected)
        {
            outErrorList->emplace_back(entryFieldErrorTypes.emptyField, QString());
            isEmptyFieldDetected = true;
        }
        else
        {
            const bool isDuplicate = processedFields.find(fieldValue) != processedFields.cend();
            const bool isDuplicateAlreadyProcessed =
                processedFieldDuplicates.find(fieldValue) != processedFieldDuplicates.cend();

            if (isDuplicate && !isDuplicateAlreadyProcessed)
            {
                outErrorList->emplace_back(
                    entryFieldErrorTypes.duplicatedField,
                    lm("%1 id: %2, %3 name: %4").args(
                        entryFieldErrorTypes.listEntryTypeName,
                        entry.id,
                        entryFieldErrorTypes.listEntryTypeName,
                        entry.name));

                processedFieldDuplicates.insert(fieldValue);
            }

            processedFields.insert(fieldValue);
        }
    }
}

template<typename List>
void validateList(
    std::vector<ManifestError>* outErrorList,
    const List& list,
    const ListManifestErrorTypes& manifestErrorTypes)
{
    if (!NX_ASSERT(outErrorList))
        return;

    validateListByField(
        outErrorList,
        list,
        {
            manifestErrorTypes.emptyId,
            manifestErrorTypes.duplicatedId,
            manifestErrorTypes.listEntryTypeName
        },
        [](const auto& entry) { return entry.id; });

    validateListByField(
        outErrorList,
        list,
        {
            manifestErrorTypes.emptyName,
            manifestErrorTypes.duplicatedName,
            manifestErrorTypes.listEntryTypeName
        },
        [](const auto& entry) { return entry.name; });
}

static bool isPixelFormatSpecified(const EngineManifest& manifest)
{
    using Capability = EngineManifest::Capability;

    return manifest.capabilities.testFlag(Capability::needUncompressedVideoFrames_yuv420)
        || manifest.capabilities.testFlag(Capability::needUncompressedVideoFrames_rgba)
        || manifest.capabilities.testFlag(Capability::needUncompressedVideoFrames_rgb)
        || manifest.capabilities.testFlag(Capability::needUncompressedVideoFrames_bgra)
        || manifest.capabilities.testFlag(Capability::needUncompressedVideoFrames_bgr)
        || manifest.capabilities.testFlag(Capability::needUncompressedVideoFrames_argb)
        || manifest.capabilities.testFlag(Capability::needUncompressedVideoFrames_abgr);
}

static void validatePixelFormats(
    std::vector<ManifestError>* inOutErrorList,
    const EngineManifest& manifest)
{
    const bool pixelFormatIsSpecified = isPixelFormatSpecified(manifest);

    if (manifest.streamTypeFilter.testFlag(StreamType::uncompressedVideo)
        && !pixelFormatIsSpecified)
    {
        inOutErrorList->push_back(
            ManifestError(ManifestErrorType::uncompressedFramePixelFormatIsNotSpecified));
    }
    else if (manifest.streamTypeFilter
        && !manifest.streamTypeFilter.testFlag(StreamType::uncompressedVideo)
        && pixelFormatIsSpecified)
    {
        inOutErrorList->push_back(
            ManifestError(ManifestErrorType::excessiveUncompressedFramePixelFormatSpecification));
    }
}

std::vector<ManifestError> validate(const EngineManifest& manifest)
{
    std::vector<ManifestError> result;
    validateList(
        &result,
        manifest.eventTypes,
        {
            ManifestErrorType::emptyEventTypeId,
            ManifestErrorType::emptyEventTypeName,
            ManifestErrorType::duplicatedEventTypeId,
            ManifestErrorType::duplicatedEventTypeName,
            "Event Type"
        });

    validateList(
        &result,
        manifest.objectTypes,
        {
            ManifestErrorType::emptyObjectTypeId,
            ManifestErrorType::emptyObjectTypeName,
            ManifestErrorType::duplicatedObjectTypeId,
            ManifestErrorType::duplicatedObjectTypeName,
            "Object Type"
        });

    validateList(
        &result,
        manifest.groups,
        {
            ManifestErrorType::emptyGroupId,
            ManifestErrorType::emptyGroupName,
            ManifestErrorType::duplicatedGroupId,
            ManifestErrorType::duplicatedGroupName,
            "Group"
        });

    validateList(
        &result,
        manifest.objectActions,
        {
            ManifestErrorType::emptyObjectActionId,
            ManifestErrorType::emptyObjectActionName,
            ManifestErrorType::duplicatedObjectActionId,
            ManifestErrorType::duplicatedObjectActionName,
            "Object Action"
        });

    validatePixelFormats(&result, manifest);

    return result;
}

} // namespace nx::vms::api::analytics
