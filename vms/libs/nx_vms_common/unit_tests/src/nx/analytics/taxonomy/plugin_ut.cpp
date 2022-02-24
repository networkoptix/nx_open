// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <QtCore/QFile>

#include <nx/analytics/taxonomy/support/utils.h>
#include <nx/analytics/taxonomy/state_compiler.h>

#include <nx/fusion/model_functions.h>

using namespace nx::vms::api::analytics;

namespace nx::analytics::taxonomy {

class PluginTest: public ::testing::Test
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

    void makeSurePluginsAreCorrect()
    {
        const std::vector<AbstractPlugin*> plugins = m_result.state->plugins();

        ASSERT_EQ(plugins.size(), m_expectedData.size());

        for (const AbstractPlugin* plugin: plugins)
        {
            PluginDescriptor expectedData = m_expectedData[plugin->id()];

            ASSERT_EQ(plugin->id(), expectedData.id);
            ASSERT_EQ(plugin->name(), expectedData.name);
        }
    }

private:
    Descriptors m_descriptors;
    std::map<QString, PluginDescriptor> m_expectedData;
    StateCompiler::Result m_result;
};

TEST_F(PluginTest, pluginsAreCorrect)
{
    givenDescriptors(":/content/taxonomy/plugin_test.json");
    afterDescriptorsCompilation();
    makeSurePluginsAreCorrect();
}

} // namespace nx::analytics::taxoonomy
