#include "device_agent_manifest.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api::analytics {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(DeviceAgentManifest, (json), DeviceAgentManifest_Fields,
    (brief, true))

template<typename ListOfTypes, typename FieldValueGetter>
void validateSupportedTypesByField(
    std::vector<ManifestError>* outErrorList,
    const QList<QString>& supportedTypeList,
    const ListOfTypes&  declaredTypeList,
    const EntryFieldManifestErrorTypes& entryFieldManifestErrorTypes,
    FieldValueGetter fieldValueGetter)
{
    bool isEmptyFieldDetected = false;
    std::set<QString> processedFields;
    std::set<QString> processedFieldDuplicates;

    if (!NX_ASSERT(outErrorList))
        return;

    const auto identitiyGetter = [](const auto& entry) { return entry; };

    const auto processList =
        [
            &isEmptyFieldDetected,
            &processedFields,
            &processedFieldDuplicates,
            &entryFieldManifestErrorTypes,
            outErrorList
        ](
            const auto& entryList,
            const auto& fieldValueGetter,
            const auto& errorMaker)
        {
            for (const auto entry: entryList)
            {
                const auto fieldValue = fieldValueGetter(entry);
                if (fieldValue.isEmpty() && !isEmptyFieldDetected)
                {
                    outErrorList->emplace_back(entryFieldManifestErrorTypes.emptyField, QString());
                    isEmptyFieldDetected = true;
                }
                else
                {
                    const bool isDuplicate =
                        processedFields.find(fieldValue) != processedFields.cend();

                    const bool isDuplicateAlreadyProcessed =
                        processedFieldDuplicates.find(fieldValue)
                            != processedFieldDuplicates.cend();

                    if (isDuplicate && !isDuplicateAlreadyProcessed)
                    {
                        outErrorList->emplace_back(
                            errorMaker(
                                entryFieldManifestErrorTypes.duplicatedField,
                                entryFieldManifestErrorTypes.listEntryTypeName,
                                entry));

                        processedFieldDuplicates.insert(fieldValue);
                    }

                    processedFields.insert(fieldValue);
                }
            }
        };

    processList(
        supportedTypeList,
        identitiyGetter,
        [](ManifestErrorType errorType, const QString& entryTypeName, const QString& entryId)
        {
            return ManifestError(errorType, lm("%1 id: %2").args(entryTypeName, entryId));
        });

    processList(
        declaredTypeList,
        fieldValueGetter,
        [](ManifestErrorType errorType, const QString& entryTypeName, const auto& entry)
        {
            return ManifestError(
                errorType,
                lm("%1 id: %2, %3 name: %4")
                    .args(entryTypeName, entry.id, entryTypeName, entry.name));
        });
}

template<typename ListOfTypes>
void validateSupportedTypes(
    std::vector<ManifestError>* outErrorList,
    const QList<QString>& supportedTypeList,
    const ListOfTypes& declaredTypeList,
    const ListManifestErrorTypes& manifestErrorTypes)
{
    if (!NX_ASSERT(outErrorList))
        return;

    validateSupportedTypesByField(
        outErrorList,
        supportedTypeList,
        declaredTypeList,
        {
            manifestErrorTypes.emptyId,
            manifestErrorTypes.duplicatedId,
            manifestErrorTypes.listEntryTypeName
        },
        [](const auto& entry) { return entry.id; });

    validateSupportedTypesByField(
        outErrorList,
        supportedTypeList,
        declaredTypeList,
        {
            manifestErrorTypes.emptyName,
            manifestErrorTypes.duplicatedName,
            manifestErrorTypes.listEntryTypeName
        },
        [](const auto& entry) { return entry.name; });
}

std::vector<ManifestError> validate(const DeviceAgentManifest& deviceAgentManifest)
{
    std::vector<ManifestError> result;
    validateSupportedTypes(
        &result,
        deviceAgentManifest.supportedEventTypeIds,
        deviceAgentManifest.eventTypes,
        {
            ManifestErrorType::emptyEventTypeId,
            ManifestErrorType::emptyEventTypeName,
            ManifestErrorType::duplicatedEventTypeId,
            ManifestErrorType::duplicatedEventTypeName,
            "Event Type"
        });

    validateSupportedTypes(
        &result,
        deviceAgentManifest.supportedObjectTypeIds,
        deviceAgentManifest.objectTypes,
        {
            ManifestErrorType::emptyObjectTypeId,
            ManifestErrorType::emptyObjectTypeName,
            ManifestErrorType::duplicatedObjectTypeId,
            ManifestErrorType::duplicatedObjectTypeName,
            "Object Type"
        });

    validateSupportedTypes(
        &result,
        QList<QString>(),
        deviceAgentManifest.groups,
        {
            ManifestErrorType::emptyGroupId,
            ManifestErrorType::emptyGroupName,
            ManifestErrorType::duplicatedGroupId,
            ManifestErrorType::duplicatedGroupName,
            "Group"
        });

    return result;
}

} // namespace nx::vms::api::analytics
