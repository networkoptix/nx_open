#include "cloud_data_provider.h"

#include <ostream>

#include <nx/utils/log/log.h>
#include <nx/utils/log/log_message.h>
#include <nx/utils/std/cpp14.h>

namespace nx {
namespace hpm {

AbstractCloudDataProvider::~AbstractCloudDataProvider()
{
}


static AbstractCloudDataProviderFactory::FactoryFunc cloudDataProviderFactoryFunc;

std::unique_ptr<AbstractCloudDataProvider> AbstractCloudDataProviderFactory::create(
    const boost::optional<nx::utils::Url>& cdbUrl,
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
    const boost::optional< AbstractCloudDataProvider::System >& system )
{
    if (!system)
        return os << "boost::none";

    return os << "System(key=" << system->authKey.data()
        << ", mediatorEnabled=" << system->mediatorEnabled << ")";
}

// impl

const std::chrono::milliseconds CloudDataProvider::DEFAULT_UPDATE_INTERVAL
    = std::chrono::minutes( 5 );

static nx::cdb::api::ConnectionFactory* makeConnectionFactory(
    const boost::optional<nx::utils::Url>& cdbUrl)
{
    auto factory = createConnectionFactory();
    if (factory && cdbUrl)
    {
        NX_LOG(lm("nx::cdb::api::ConnectionFactory set url %1").arg(cdbUrl), cl_logALWAYS);
        factory->setCloudUrl(cdbUrl->toString().toStdString());
    }
    return factory;
}

CloudDataProvider::CloudDataProvider(
    const boost::optional<nx::utils::Url>& cdbUrl,
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

boost::optional< AbstractCloudDataProvider::System >
    CloudDataProvider::getSystem(const String& systemId) const
{
    QnMutexLocker lk(&m_mutex);
    const auto it = m_systemCache.find(systemId);
    if (it == m_systemCache.end())
        return boost::none;

    return it->second;
}

static QString traceSystems(const std::map< String, AbstractCloudDataProvider::System >& systems)
{
    QStringList list;
    for (const auto sys: systems)
    {
        list << lm("%1 (%2 %3)").arg(sys.first)
            .arg(sys.second.authKey)
            .arg(sys.second.mediatorEnabled);
    }
    return list.join(QLatin1String(", "));
}

void CloudDataProvider::updateSystemsAsync()
{
    m_connection->systemManager()->getSystemsFiltered(
        cdb::api::Filter(/* Empty filter to get all systems independent of customization. */),
        [this](cdb::api::ResultCode code, cdb::api::SystemDataExList systems)
        {
            if (code != cdb::api::ResultCode::ok)
            {
                NX_LOGX(lm("Error: %1").arg(m_connectionFactory->toString(code)),
                    (std::chrono::steady_clock::now() - m_startTime > m_startTimeout)
                        ? cl_logERROR : cl_logDEBUG1);

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

                NX_LOGX(lm("There is(are) %1 system(s) updated")
                    .arg(systems.systems.size()), cl_logDEBUG1);
                NX_LOGX(lm("Updated: %1")
                    .arg(traceSystems(m_systemCache)), cl_logDEBUG2);
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
