#include "cloud_data_provider.h"

#include <ostream>

#include <nx/utils/log/log.h>
#include <nx/utils/log/log_message.h>
#include <nx/utils/std/cpp14.h>

namespace nx {
namespace hpm {

static AbstractCloudDataProviderFactory::FactoryFunc cloudDataProviderFactoryFunc;

std::unique_ptr<AbstractCloudDataProvider> AbstractCloudDataProviderFactory::create(
    const std::optional<nx::utils::Url>& cdbUrl,
    const std::string& user,
    const std::string& password,
    std::chrono::milliseconds updateInterval,
    std::chrono::milliseconds startTimeout)
{
    if (cloudDataProviderFactoryFunc)
    {
        return cloudDataProviderFactoryFunc(
            cdbUrl,
            user,
            password,
            updateInterval,
            startTimeout);
    }
    else
    {
        return std::make_unique<CloudDataProvider>(
            cdbUrl,
            user,
            password,
            updateInterval,
            startTimeout);
    }
}

AbstractCloudDataProviderFactory::FactoryFunc
    AbstractCloudDataProviderFactory::setFactoryFunc(FactoryFunc factoryFunc)
{
    auto prevFunc = std::move(cloudDataProviderFactoryFunc);
    cloudDataProviderFactoryFunc = std::move(factoryFunc);
    return prevFunc;
}


AbstractCloudDataProvider::System::System():
    mediatorEnabled(false)
{
}

AbstractCloudDataProvider::System::System(String authKey_, bool mediatorEnabled_):
    authKey(std::move(authKey_)),
    mediatorEnabled(mediatorEnabled_)
{
}

AbstractCloudDataProvider::System::System(
    String id_,
    String authKey_,
    bool mediatorEnabled_)
:
    id(std::move(id_)),
    authKey(std::move(authKey_)),
    mediatorEnabled(mediatorEnabled_)
{
}

std::ostream& operator<<(
    std::ostream& os,
    const std::optional< AbstractCloudDataProvider::System >& system )
{
    if (!system)
        return os << "std::none";

    return os << "System(key=" << system->authKey.data()
        << ", mediatorEnabled=" << system->mediatorEnabled << ")";
}

// impl

const std::chrono::milliseconds CloudDataProvider::DEFAULT_UPDATE_INTERVAL
    = std::chrono::minutes( 5 );

static nx::cloud::db::api::ConnectionFactory* makeConnectionFactory(
    const std::optional<nx::utils::Url>& cdbUrl)
{
    auto factory = createConnectionFactory();
    if (factory && cdbUrl)
    {
        NX_ALWAYS(typeid(nx::cloud::db::api::ConnectionFactory), lm("set url %1"), cdbUrl);
        factory->setCloudUrl(cdbUrl->toString().toStdString());
    }
    return factory;
}

CloudDataProvider::CloudDataProvider(
    const std::optional<nx::utils::Url>& cdbUrl,
    const std::string& user,
    const std::string& password,
    std::chrono::milliseconds updateInterval,
    std::chrono::milliseconds startTimeout)
:
    m_updateInterval(std::move(updateInterval)),
    m_startTime(std::chrono::steady_clock::now()),
    m_startTimeout(startTimeout),
    m_isTerminated(false),
    m_connectionFactory(makeConnectionFactory(cdbUrl)),
    m_connection(m_connectionFactory->createConnection())
{
    m_connection->setCredentials(user, password);
    updateSystemsAsync();
}

CloudDataProvider::~CloudDataProvider()
{
    QnMutexLocker lk(&m_mutex);
    m_isTerminated = true;

    const auto connection = std::move(m_connection);
    const auto guard = std::move(m_timerGuard);

    lk.unlock();
}

std::optional< AbstractCloudDataProvider::System >
    CloudDataProvider::getSystem(const String& systemId) const
{
    QnMutexLocker lk(&m_mutex);
    const auto it = m_systemCache.find(systemId);
    if (it == m_systemCache.end())
        return std::nullopt;

    return it->second;
}

void CloudDataProvider::updateSystemsAsync()
{
    m_connection->systemManager()->getSystemsFiltered(
        nx::cloud::db::api::Filter(/* Empty filter to get all systems independent of customization. */),
        [this](nx::cloud::db::api::ResultCode code, nx::cloud::db::api::SystemDataExList systems)
        {
            if (code != nx::cloud::db::api::ResultCode::ok)
            {
                NX_UTILS_LOG((std::chrono::steady_clock::now() - m_startTime > m_startTimeout)
                    ? utils::log::Level::error
                    : utils::log::Level::debug,
                    this, lm("Error: %1").arg(m_connectionFactory->toString(code)));
                // TODO: shall we m_systemCache.clear() after a few failing attempts?
            }
            else
            {
                QnMutexLocker lk(&m_mutex);
                m_systemCache.clear();
                for (auto& sys : systems.systems)
                {
                    m_systemCache.emplace(
                        sys.id.c_str(),
                        System(String(sys.authKey.c_str()),
                            sys.cloudConnectionSubscriptionStatus));
                }

                NX_DEBUG(this, lm("There is(are) %1 system(s) updated").arg(systems.systems.size()));
            }

            QnMutexLocker lk(&m_mutex);
            if (!m_isTerminated)
            {
                m_timerGuard =
                    nx::utils::TimerManager::instance()->addTimerEx(
                        [this](quint64) { updateSystemsAsync(); }, m_updateInterval);
            }
        });
}

} // namespace hpm
} // namespace nx
