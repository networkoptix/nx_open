// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <QtCore/QFile>
#include <QtCore/QJsonObject>

#include <nx/analytics/taxonomy/state_compiler.h>
#include <nx/reflect/json/deserializer.h>
#include <nx/vms/common/test_support/analytics/taxonomy/test_resource_support_proxy.h>
#include <nx/vms/common/test_support/analytics/taxonomy/utils.h>

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

        const QJsonObject object = testData.fullData["result"].toObject();
        const QByteArray obectAsBytes = QJsonDocument(object).toJson();

        auto [deserializationResult, result] =
            nx::reflect::json::deserialize<std::map<QString, PluginDescriptor>>(obectAsBytes.toStdString());

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
