// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "engine_manifest.h"

#include <set>

#include <nx/fusion/model_functions.h>

namespace nx::vms::api::analytics {

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
                    nx::format("%1 id: %2, %3 name: %4").args(
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
    using Capability = EngineCapability;

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
