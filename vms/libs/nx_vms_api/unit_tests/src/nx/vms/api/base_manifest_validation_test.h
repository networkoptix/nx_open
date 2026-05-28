// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <gtest/gtest.h>

#include <nx/utils/log/log.h>
#include <nx/vms/api/analytics/manifest_error.h>

namespace nx::vms::api::analytics {

inline std::vector<std::string> makeStrings(const std::string& prefix)
{
    std::vector<std::string> result;
    const int stringCount = 10;
    result.reserve(stringCount);
    for (int i = 0; i < stringCount; ++i)
        result.push_back(prefix + std::to_string(i));

    return result;
}

template<typename Entry>
std::vector<Entry> makeEntries(const std::string& entryTypeName)
{
    std::vector<Entry> result;
    const int entryCount = 10;
    result.reserve(entryCount);
    for (int i = 0; i < entryCount; ++i)
    {
        Entry eventType;
        eventType.id = std::string("testId") + entryTypeName + std::to_string(i);
        eventType.name = entryTypeName + " test name " + std::to_string(i);
        result.push_back(eventType);
    }

    return result;
}

template<typename Entry, typename TransformationFunc>
void transformSomeEntries(
    std::vector<Entry>* inOutEntries,
    int numberOfEntriesToTransform,
    TransformationFunc transformationFunc)
{
    if (!NX_ASSERT(inOutEntries))
        return;

    for (int i = 0; i < std::min((int)inOutEntries->size(), numberOfEntriesToTransform); ++i)
        transformationFunc(&(*inOutEntries)[i]);
}

template<typename Manifest>
class BaseManifestValidationTest: public ::testing::Test
{
protected:
    void whenValidatingManifest()
    {
        m_validationResult = validate(m_manifest);
    }

    void makeSureManifestIsCorrect()
    {
        ASSERT_TRUE(m_validationResult.empty());
    }

    void makeSureErrorsAreCaught(std::vector<ManifestErrorType> errorTypesThatShouldBeCaught)
    {
        std::set<ManifestErrorType> validationErrors;
        for (const auto& error: m_validationResult)
            validationErrors.insert(error.errorType);

        for (const auto& errorType: errorTypesThatShouldBeCaught)
            ASSERT_TRUE(validationErrors.find(errorType) != validationErrors.cend());
    }

protected:
    Manifest m_manifest;
    std::vector<ManifestError> m_validationResult;
};

} // namespace nx::vms::api::analytics
