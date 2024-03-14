// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "device_agent_manifest.h"

#include <set>

#include <nx/fusion/model_functions.h>

namespace nx::vms::api::analytics {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(DeviceAgentManifest, (json), DeviceAgentManifest_Fields,
    (brief, true))

class ListProcessor
{
public:
    ListProcessor(EntryFieldManifestErrorTypes manifestErrorTypes):
        m_manifestErrorTypes(std::move(manifestErrorTypes))
    {
    }

    template<typename Entity, typename FieldValueGetter, typename ErrorMaker>
    void process(
        std::vector<ManifestError>* inOutErrorList,
        const QList<Entity>& entityList,
        FieldValueGetter fieldValueGetter,
        ErrorMaker errorMaker)
    {
        if (!NX_ASSERT(inOutErrorList))
            return;

        for (const auto& entity: entityList)
        {
            const auto fieldValue = fieldValueGetter(entity);
            if (fieldValue.isEmpty() && !m_isEmptyFieldDetected)
            {
                inOutErrorList->emplace_back(m_manifestErrorTypes.emptyField, QString());
                m_isEmptyFieldDetected = true;
            }
            else
            {
                const bool isDuplicate =
                    m_processedFields.find(fieldValue) != m_processedFields.cend();

                const bool isDuplicateAlreadyProcessed =
                    m_processedFieldDuplicates.find(fieldValue)
                    != m_processedFieldDuplicates.cend();

                if (isDuplicate && !isDuplicateAlreadyProcessed)
                {
                    inOutErrorList->emplace_back(
                        errorMaker(
                            m_manifestErrorTypes.duplicatedField,
                            m_manifestErrorTypes.listEntryTypeName,
                            entity));

                    m_processedFieldDuplicates.insert(fieldValue);
                }

                m_processedFields.insert(fieldValue);
            }
        }
    }

private:
    bool m_isEmptyFieldDetected = false;
    std::set<QString> m_processedFields;
    std::set<QString> m_processedFieldDuplicates;
    EntryFieldManifestErrorTypes m_manifestErrorTypes;
};

template<typename ListOfTypes, typename FieldValueGetter>
void validateSupportedTypesByField(
    std::vector<ManifestError>* outErrorList,
    const QList<QString>& supportedTypeList,
    const ListOfTypes&  declaredTypeList,
    const EntryFieldManifestErrorTypes& entryFieldManifestErrorTypes,
    FieldValueGetter fieldValueGetter)
{
    if (!NX_ASSERT(outErrorList))
        return;

    const auto identitiyGetter = [](const auto& entry) { return entry; };

    ListProcessor listProcessor(entryFieldManifestErrorTypes);

    listProcessor.process(
        outErrorList,
        supportedTypeList,
        identitiyGetter,
        [](ManifestErrorType errorType, const QString& entryTypeName, const QString& entryId)
        {
            return ManifestError(errorType, nx::format("%1 id: %2").args(entryTypeName, entryId));
        });

    listProcessor.process(
        outErrorList,
        declaredTypeList,
        fieldValueGetter,
        [](ManifestErrorType errorType, const QString& entryTypeName, const auto& entry)
        {
            return ManifestError(
                errorType,
                nx::format("%1 id: %2, %3 name: %4")
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

    if (!deviceAgentManifest.deviceAgentSettingsModel.isUndefined()
        && !deviceAgentManifest.deviceAgentSettingsModel.isNull()
        && !deviceAgentManifest.deviceAgentSettingsModel.isObject())
    {
        result.emplace_back(ManifestErrorType::deviceAgentSettingsModelIsIncorrect);
    }

    return result;
}

} // namespace nx::vms::api::analytics
