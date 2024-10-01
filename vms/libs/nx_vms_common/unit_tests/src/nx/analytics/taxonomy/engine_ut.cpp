// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <optional>

#include <gtest/gtest.h>

#include <QtCore/QFile>

#include <nx/analytics/taxonomy/state_compiler.h>
#include <nx/reflect/json/deserializer.h>
#include <nx/vms/common/test_support/analytics/taxonomy/test_resource_support_proxy.h>
#include <nx/vms/common/test_support/analytics/taxonomy/utils.h>

using namespace nx::vms::api::analytics;

namespace nx::analytics::taxonomy {

struct EngineTestExpectedData
{
    QString id;
    QString name;
    EngineCapabilities capabilities;
    std::optional<PluginDescriptor> plugin;
};
#define EngineTestExpectedData_Fields \
    (id) \
    (name) \
    (capabilities) \
    (plugin)

NX_REFLECTION_INSTRUMENT(EngineTestExpectedData, EngineTestExpectedData_Fields);

class EngineTest: public ::testing::Test
{
protected:
    void givenDescriptors(const QString& filePath)
    {
        TestData testData;
        ASSERT_TRUE(loadDescriptorsTestData(filePath, &testData));
        m_descriptors = std::move(testData.descriptors);

        const QJsonObject object = testData.fullData["result"].toObject();
        const QByteArray objectAsBytes = QJsonDocument(object).toJson();

        auto [deserializationResult, result] =
            nx::reflect::json::deserialize<std::map<QString, EngineTestExpectedData> >(objectAsBytes.toStdString());

        m_expectedData = deserializationResult;

        ASSERT_TRUE(result.success);

        ASSERT_FALSE(m_expectedData.empty());
    }

    void afterDescriptorsCompilation()
    {
        m_result = StateCompiler::compile(
            m_descriptors,
            std::make_unique<TestResourceSupportProxy>());
    }

    void makeSureEnginesAreCorrect()
    {
        const std::vector<AbstractEngine*> engines = m_result.state->engines();

        ASSERT_EQ(engines.size(), m_expectedData.size());

        for (const AbstractEngine* engine: engines)
        {
            const EngineTestExpectedData& expectedData = m_expectedData[engine->id()];

            ASSERT_EQ(engine->id(), expectedData.id);
            ASSERT_EQ(engine->name(), expectedData.name);
            ASSERT_EQ(engine->capabilities(), expectedData.capabilities);

            const AbstractIntegration* integration = engine->integration();
            if (expectedData.plugin)
            {
                ASSERT_EQ(integration->id(), expectedData.plugin->id);
                ASSERT_EQ(integration->name(), expectedData.plugin->name);
            }
            else
            {
                ASSERT_EQ(integration, nullptr);
            }
        }
    }

private:
    Descriptors m_descriptors;
    std::map<QString, EngineTestExpectedData> m_expectedData;
    StateCompiler::Result m_result;
};

TEST_F(EngineTest, enginesAreCorrect)
{
    givenDescriptors(":/content/taxonomy/engine_test.json");
    afterDescriptorsCompilation();
    makeSureEnginesAreCorrect();
}

} // namespace nx::analytics::taxoonomy
