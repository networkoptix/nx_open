// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <optional>

#include <QtCore/QFile>

#include <nx/analytics/taxonomy/support/utils.h>
#include <nx/analytics/taxonomy/state_compiler.h>

#include <nx/fusion/model_functions.h>

using namespace nx::vms::api::analytics;

namespace nx::analytics::taxonomy {

struct EngineTestExpectedData
{
    QString id;
    QString name;
    EngineManifest::Capabilities capabilities;
    std::optional<PluginDescriptor> plugin;
};
#define EngineTestExpectedData_Fields \
    (id) \
    (name) \
    (capabilities) \
    (plugin)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(EngineTestExpectedData, (json),
    EngineTestExpectedData_Fields, (brief, true))

class EngineTest: public ::testing::Test
{
protected:
    void givenDescriptors(const QString& filePath)
    {
        TestData testData;
        ASSERT_TRUE(loadDescriptorsTestData(filePath, &testData));
        m_descriptors = std::move(testData.descriptors);
        ASSERT_TRUE(QJson::deserialize(testData.fullData["result"].toObject(), &m_expectedData));
        ASSERT_FALSE(m_expectedData.empty());
    }

    void afterDescriptorsCompilation()
    {
        m_result = StateCompiler::compile(m_descriptors);
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

            const AbstractPlugin* plugin = engine->plugin();
            if (expectedData.plugin)
            {
                ASSERT_EQ(plugin->id(), expectedData.plugin->id);
                ASSERT_EQ(plugin->name(), expectedData.plugin->name);
            }
            else
            {
                ASSERT_EQ(plugin, nullptr);
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
