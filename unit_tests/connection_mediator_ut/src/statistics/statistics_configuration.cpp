#include <boost/optional.hpp>

#include <gtest/gtest.h>

#include <statistics/collector.h>

#include "../functional_tests/mediator_functional_test.h"

namespace nx {
namespace hpm {
namespace stats {
namespace test {

class SettingsConfiguration:
    public hpm::test::MediatorFunctionalTest
{
public:
    SettingsConfiguration()
    {
        using namespace std::placeholders;

        m_factoryFuncBak = CollectorFactory::instance().setCustomFunc(
            std::bind(&SettingsConfiguration::createCollector, this, _1, _2));
    }

    ~SettingsConfiguration()
    {
        if (m_factoryFuncBak)
            CollectorFactory::instance().setCustomFunc(std::move(*m_factoryFuncBak));
    }

protected:
    void whenStartedMediator()
    {
        ASSERT_TRUE(startAndWaitUntilStarted());
    }

    void thenStatisticsEnabled()
    {
        ASSERT_TRUE(m_collectorCreated);
    }

    void thenStatisticsNotEnabled()
    {
        ASSERT_FALSE(m_collectorCreated);
    }

private:
    boost::optional<CollectorFactory::Function> m_factoryFuncBak;
    bool m_collectorCreated = false;

    std::unique_ptr<AbstractCollector> createCollector(
        const conf::Statistics& settings,
        nx::utils::db::AsyncSqlQueryExecutor* sqlQueryExecutor)
    {
        m_collectorCreated = true;
        return (*m_factoryFuncBak)(settings, sqlQueryExecutor);
    }
};

//-------------------------------------------------------------------------------------------------

TEST_F(SettingsConfiguration, statistics_enabled_by_default)
{
    whenStartedMediator();
    thenStatisticsEnabled();
}

TEST_F(SettingsConfiguration, statistics_enabled_explicitly)
{
    addArg("--stats/enabled=true");

    whenStartedMediator();
    thenStatisticsEnabled();
}

TEST_F(SettingsConfiguration, statistics_can_be_disabled)
{
    addArg("--stats/enabled=false");

    whenStartedMediator();
    thenStatisticsNotEnabled();
}

} // namespace test
} // namespace stats
} // namespace hpm
} // namespace nx
