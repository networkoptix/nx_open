/**********************************************************
* Sep 3, 2015
* akolesnikov
***********************************************************/

#include "system_manager.h"

#include "data/system_data.h"



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
    QUrl urlToExecute = url();
    urlToExecute.setPath(urlToExecute.path() + "/system/bind");
    execute(
        urlToExecute,
        registrationData,
        std::move(completionHandler));
}

void SystemManager::unbindSystem(
    const std::string& systemID,
    std::function<void(api::ResultCode)> completionHandler)
{
    QUrl urlToExecute = url();
    urlToExecute.setPath(urlToExecute.path() + "/system/unbind");
    execute(
        urlToExecute,
        api::SystemID(systemID),
        std::move(completionHandler));
}

void SystemManager::getSystems(
    std::function<void(api::ResultCode, api::SystemDataList)> completionHandler)
{
    QUrl urlToExecute = url();
    urlToExecute.setPath(urlToExecute.path() + "/system/get");
    execute(
        urlToExecute,
        std::move(completionHandler));
}

void SystemManager::shareSystem(
    api::SystemSharing sharingData,
    std::function<void(api::ResultCode)> completionHandler)
{
    QUrl urlToExecute = url();
    urlToExecute.setPath(urlToExecute.path() + "/system/share");
    execute(
        urlToExecute,
        sharingData,
        std::move(completionHandler));
}

void SystemManager::setCredentials(
    const std::string& login,
    const std::string& password)
{
    QnMutexLocker lk(&m_mutex);
    m_url.setUserName(QString::fromStdString(login));
    m_url.setPassword(QString::fromStdString(password));
}

QUrl SystemManager::url() const
{
    QnMutexLocker lk(&m_mutex);
    return m_url;
}

}   //cl
}   //cdb
}   //nx
