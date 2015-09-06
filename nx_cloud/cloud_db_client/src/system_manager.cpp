/**********************************************************
* Sep 3, 2015
* akolesnikov
***********************************************************/

#include "system_manager.h"


namespace nx {
namespace cdb {
namespace cl {

SystemManager::SystemManager(QUrl url)
:
    m_url(std::move(url))
{
}

void SystemManager::bindSystem(
    api::SystemRegistrationData registrationData,
    std::function<void(api::ResultCode, api::SystemData)> completionHandler)
{
    //TODO #ak
    completionHandler(api::ResultCode::notImplemented, api::SystemData());
}

void SystemManager::unbindSystem(
    const std::string& systemID,
    std::function<void(api::ResultCode)> completionHandler)
{
    //TODO #ak
    completionHandler(api::ResultCode::notImplemented);
}

void SystemManager::getSystems(
    std::function<void(api::ResultCode, std::vector<api::SystemData>)> completionHandler)
{
    //TODO #ak
    completionHandler(
        api::ResultCode::notImplemented,
        std::vector<api::SystemData>());
}

void SystemManager::shareSystem(
    api::SystemSharing sharingData,
    std::function<void(api::ResultCode)> completionHandler)
{
    //TODO #ak
    completionHandler(api::ResultCode::notImplemented);
}

void SystemManager::setCredentials(
    const std::string& login,
    const std::string& password)
{
    QnMutexLocker lk(&m_mutex);
    m_url.setUsername(login);
    m_url.setPassword(password);
}

QUrl SystemManager::getUrl() const
{
    QnMutexLocker lk(&m_mutex);
    return m_url;
}

}   //cl
}   //cdb
}   //nx
