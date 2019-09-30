#pragma once

#include <gtest/gtest.h>

#include <nx/vms/api/analytics/manifest_error.h>

namespace nx::vms::api::analytics {

inline QList<QString> makeStrings(const QString& prefix)
{
    QList<QString> result;
    for (int i = 0; i < 10; ++i)
        result.push_back(prefix + QString::number(i));

    return result;
}

template<typename Entry>
QList<Entry> makeEntries(const QString& entryTypeName)
{
    QList<Entry> result;
    for (int i = 0; i < 10; ++i)
    {
        Entry eventType;
        eventType.id = "testId" + entryTypeName + QString::number(i);
        eventType.name = entryTypeName + " test name " + QString::number(i);
        result.push_back(eventType);
    }

    return result;
}

template<typename Entry, typename TransformationFunc>
void transformSomeEntries(
    QList<Entry>* inOutEntries,
    int numberOfEntriesToTransform,
    TransformationFunc transformationFunc)
{
    if (!NX_ASSERT(inOutEntries))
        return;

    for (int i = 0; i < std::min(inOutEntries->size(), numberOfEntriesToTransform); ++i)
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
