#include <gtest/gtest.h>

#include <nx/analytics/properties.h>
#include <nx/analytics/plugin_descriptor_manager.h>
#include <nx/core/access/access_types.h>
#include <nx/core/resource/server_mock.h>
#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>

namespace nx::analytics {

using namespace nx::vms::api::analytics;

static const int kServerCount = 10;
static const QString kUpdatedPluginPostfix = "_updated";

QString makePluginId(int pluginIndex)
{
    return lm("plugin.%1").args(pluginIndex);
}

QString makePluginName(int pluginIndex)
{
    return lm("Plugin %1").args(pluginIndex);
}

PluginManifest makePluginManifest(int pluginIndex, const QString& postfix = QString())
{
    PluginManifest pluginManifest;
    pluginManifest.id = makePluginId(pluginIndex);
    pluginManifest.name = makePluginName(pluginIndex) + postfix;

    return pluginManifest;
}

PluginDescriptor makePluginDescriptor(int pluginIndex, const QString& postfix = QString())
{
    PluginDescriptor pluginDescriptor;
    pluginDescriptor.id = makePluginId(pluginIndex);
    pluginDescriptor.name = makePluginName(pluginIndex) + postfix;

    return pluginDescriptor;
}

PluginDescriptorMap makePluginDescriptorRange(int descriptorRangeStart, int descriptorRangeEnd)
{
    PluginDescriptorMap pluginDescriptors;
    for (auto i = descriptorRangeStart; i <= descriptorRangeEnd; ++i)
        pluginDescriptors.emplace(makePluginId(i), makePluginDescriptor(i));

    return pluginDescriptors;
}

void setupPluginDescriptorsToServer(
    QnMediaServerResourcePtr server,
    const PluginDescriptorMap& descriptors)
{
    server->setProperty(
        kPluginDescriptorsProperty,
        QString::fromUtf8(QJson::serialized(descriptors)));
}

struct ServerDescriptorConfig
{
    int descriptorRangeStart = 0;
    int descriptorRangeEnd = 0;
};

struct DescriptorsToCheck
{
    std::set<int> accessibleDescriptors;
    std::set<int> absentDescriptors;

    std::set<PluginId> toPluginIds() const
    {
        std::set<PluginId> pluginIds;
        for (const auto pluginIndex: accessibleDescriptors)
            pluginIds.insert(makePluginId(pluginIndex));

        for (const auto pluginIndex: absentDescriptors)
            pluginIds.insert(makePluginId(pluginIndex));

        return pluginIds;
    }

    bool isCorrectResult(const PluginDescriptorMap& pluginDescriptorMap) const
    {
        for (const auto pluginIndex: accessibleDescriptors)
        {
            const auto it = pluginDescriptorMap.find(makePluginId(pluginIndex));
            if (it == pluginDescriptorMap.cend())
                return false;

            if (it->second != makePluginDescriptor(pluginIndex))
                return false;
        }

        for (const auto pluginIndex: absentDescriptors)
        {
            const auto it = pluginDescriptorMap.find(makePluginId(pluginIndex));
            if (it != pluginDescriptorMap.cend())
                return false;
        }

        return true;
    }
};

using ServerIndex = int;
using ServerDescriptorConfigMap = std::map<ServerIndex, ServerDescriptorConfig>;

class PluginDescriptorManagerTest: public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        makeCommonModule();
        makeServers();
    }

    void makeCommonModule()
    {
        m_commonModule = std::make_unique<QnCommonModule>(
            /*clientMode*/ false,
            nx::core::access::Mode::direct);

        m_commonModule->setModuleGUID(QnUuid::createUuid());
    }

    void makeServers()
    {
        m_servers.clear();
        for (auto i = 0; i < kServerCount; ++i)
        {
            QnMediaServerResourcePtr server(
                new nx::core::resource::ServerMock(m_commonModule.get()));

            const bool isOwnServer = i == 0;
            server->setId(isOwnServer ? m_commonModule->moduleGUID() : QnUuid::createUuid());
            m_commonModule->resourcePool()->addResource(server);
            m_servers.push_back(server);
        }

        m_servers[0]->setId(m_commonModule->moduleGUID());
    }

    void givenServersWithPredefinedDescriptors(
        const std::map<ServerIndex, ServerDescriptorConfig>& descriptorConfigsPerServer)
    {
        for (const auto& [serverIndex, descriptorConfig]: descriptorConfigsPerServer)
        {
            const bool serverIndexIsValid = serverIndex < m_servers.size();
            EXPECT_TRUE(serverIndexIsValid);
            if (!serverIndexIsValid)
                continue;

            setupPluginDescriptorsToServer(
                m_servers[serverIndex],
                makePluginDescriptorRange(
                    descriptorConfig.descriptorRangeStart,
                    descriptorConfig.descriptorRangeEnd));
        }
    }

    void givenPluginsWithManifests(const std::set<int>& pluginIndices)
    {
        PluginDescriptorManager pluginDescriptorManager(m_commonModule.get());
        for (auto pluginIndex: pluginIndices)
        {
            pluginDescriptorManager.updateFromPluginManifest(
                makePluginManifest(pluginIndex, kUpdatedPluginPostfix));
        }

    }

    void makeSureAllDescriptorsAreAccessible(
        const ServerDescriptorConfigMap& descriptorConfigsPerServer)
    {
        PluginDescriptorManager descriptorManager(m_commonModule.get());
        for (const auto& [serverIndex, descriptorConfig]: descriptorConfigsPerServer)
        {
            const bool serverIndexIsValid = serverIndex < m_servers.size();
            EXPECT_TRUE(serverIndexIsValid);
            if (!serverIndexIsValid)
                continue;

            for (auto i = descriptorConfig.descriptorRangeStart;
                i <= descriptorConfig.descriptorRangeEnd;
                ++i)
            {
                const auto descriptor = descriptorManager.descriptor(makePluginId(i));
                ASSERT_TRUE(descriptor.has_value());
                ASSERT_EQ(*descriptor, makePluginDescriptor(i));
            }
        }
    }

    void makeSureAllDescriptorsAreAccessible(
        const std::vector<DescriptorsToCheck>& descriptorsToCheck)
    {
        PluginDescriptorManager descriptorManager(m_commonModule.get());
        for (const auto& testCase: descriptorsToCheck)
        {
            const auto descriptors = descriptorManager.descriptors(testCase.toPluginIds());
            ASSERT_TRUE(testCase.isCorrectResult(descriptors));
        }
    }

    void makeSureDescriptorsHaveBeenCorrectlyUpdated(
        const std::set<int>& pluginIndices,
        const ServerDescriptorConfigMap& predefinedDescriptorConfigs)
    {
        std::set<int> allIndices = pluginIndices;
        for (const auto& [serverIndex, serverDescriptorConfig]: predefinedDescriptorConfigs)
        {
            for (auto i = serverDescriptorConfig.descriptorRangeStart;
                i <= serverDescriptorConfig.descriptorRangeEnd;
                ++i)
            {
                allIndices.insert(i);
            }
        }

        PluginDescriptorManager pluginDescriptorManager(m_commonModule.get());
        for (const auto pluginIndex: allIndices)
        {
            const auto descriptor = pluginDescriptorManager.descriptor(makePluginId(pluginIndex));
            ASSERT_TRUE(descriptor.has_value());

            const auto expectedDescriptor = pluginIndices.find(pluginIndex) == pluginIndices.cend()
                ? makePluginDescriptor(pluginIndex)
                : makePluginDescriptor(pluginIndex, kUpdatedPluginPostfix);

            ASSERT_EQ(*descriptor, expectedDescriptor);
        }
    }

protected:
    std::unique_ptr<QnCommonModule> m_commonModule;
    QnMediaServerResourceList m_servers;
};

static const ServerDescriptorConfigMap kPredefinedDescriptors = {
    {0, {2, 5}},
    {2, {3, 6}},
    {9, {1, 2}},
    {7, {8, 8}}
};

std::vector<DescriptorsToCheck> kGetDescriptorsTestCases = {
    {
        {3, 4, 5},
        {10, 12, 14}
    },
    {
        {8, 2, 1},
        {7, 9}
    },
    {
        {1, 2, 3, 4, 5, 6, 8},
        {}
    }
};

static const std::set<int> kPluginIndices = {};

TEST_F(PluginDescriptorManagerTest, getDescriptor)
{
    givenServersWithPredefinedDescriptors(kPredefinedDescriptors);
    makeSureAllDescriptorsAreAccessible(kPredefinedDescriptors);
}

TEST_F(PluginDescriptorManagerTest, getDescriptors)
{
    givenServersWithPredefinedDescriptors(kPredefinedDescriptors);
    makeSureAllDescriptorsAreAccessible(kGetDescriptorsTestCases);
}

TEST_F(PluginDescriptorManagerTest, updateFromPluginManifest)
{
    givenServersWithPredefinedDescriptors(kPredefinedDescriptors);
    givenPluginsWithManifests(kPluginIndices);
    makeSureDescriptorsHaveBeenCorrectlyUpdated(kPluginIndices, kPredefinedDescriptors);
}

} // namespace nx::analytics
