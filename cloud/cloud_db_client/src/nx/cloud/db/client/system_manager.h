// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QUrl>

#include "async_http_requests_executor.h"
#include "include/nx/cloud/db/api/system_manager.h"

namespace nx::cloud::db::client {

class SystemManager:
    public api::SystemManager
{
public:
    SystemManager(AsyncRequestsExecutor* requestsExecutor);

    virtual void bindSystem(
        api::SystemRegistrationData registrationData,
        std::function<void(api::ResultCode, api::SystemData)> completionHandler) override;

    virtual void unbindSystem(
        const std::string& systemId,
        std::function<void(api::ResultCode)> completionHandler) override;

    virtual void getSystems(
        std::function<void(api::ResultCode, api::SystemDataExList)> completionHandler ) override;

    virtual void getSystemsFiltered(
        const api::Filter& filter,
        std::function<void(api::ResultCode, api::SystemDataExList)> completionHandler) override;

    virtual void getSystem(
        const std::string& systemId,
        std::function<void(api::ResultCode, api::SystemDataEx)> completionHandler) override;

    virtual void getSystems(
        api::SystemIdList systemIdList,
        std::function<void(api::ResultCode, api::SystemDataExList)> completionHandler) override;

    virtual void shareSystem(
        const std::string& systemId,
        api::ShareSystemRequest sharingData,
        std::function<void(api::ResultCode, api::SystemSharing)> completionHandler) override;

    virtual void revokeUserAccess(
        const std::string& systemId,
        const std::string& email,
        std::function<void(api::ResultCode)> completionHandler) override;

    virtual void getCloudUsersOfSystem(
        const std::string& systemId,
        std::function<void(api::ResultCode, api::SystemSharingList)> completionHandler) override;

    virtual void saveCloudUserOfSystem(
        const std::string& systemId,
        const api::SystemSharing& userData,
        std::function<void(api::ResultCode, api::SystemSharing)> completionHandler) override;

    virtual void getAccessRoleList(
        const std::string& systemId,
        std::function<void(api::ResultCode, api::SystemAccessRoleList)> completionHandler) override;

    virtual void update(
        const std::string& systemId,
        const api::SystemAttributesUpdate& updatedData,
        std::function<void(api::ResultCode)> completionHandler) override;

    virtual void rename(
        const std::string& systemId,
        const std::string& systemName,
        std::function<void(api::ResultCode)> completionHandler) override;

    virtual void recordUserSessionStart(
        const std::string& systemId,
        std::function<void(api::ResultCode)> completionHandler) override;

    virtual void getSystemHealthHistory(
        const std::string& systemId,
        std::function<void(api::ResultCode, api::SystemHealthHistory)> completionHandler) override;

    virtual void getSystemDataSyncSettings(
        const std::string& systemId,
        std::function<void(api::ResultCode, api::DataSyncSettings)> completionHandler) override;

    virtual void startMerge(
        const std::string& idOfSystemToMergeTo,
        const std::string& idOfSystemBeingMerged,
        std::function<void(api::ResultCode)> completionHandler) override;

    virtual void startMerge(
        const std::string& idOfSystemToMergeTo,
        const api::MergeRequest& request,
        std::function<void(api::ResultCode)> completionHandler) override;

    virtual void validateMSSignature(
        const std::string& systemId,
        const api::ValidateMSSignatureRequest& request,
        std::function<void(api::ResultCode)> completionHandler) override;

    virtual void offerSystem(
        api::CreateSystemOfferRequest request,
        std::function<void(api::ResultCode)> completionHandler) override;

    virtual void acceptSystemOffer(
        const std::string& systemId,
        std::function<void(api::ResultCode)> completionHandler) override;

    virtual void rejectSystemOffer(
        const std::string& systemId,
        std::function<void(api::ResultCode)> completionHandler) override;

    virtual void deleteSystemOffer(
        const std::string& systemId,
        std::function<void(api::ResultCode)> completionHandler) override;

    virtual void getSystemOffers(
        std::function<void(api::ResultCode, std::vector<api::SystemOffer>)> completionHandler) override;

    virtual void addSystemAttributes(
        const std::string& systemId,
        const std::vector<api::Attribute>& attributes,
        std::function<void(api::ResultCode, std::vector<api::Attribute>)> completionHandler) override;

    virtual void updateSystemAttributes(
        const std::string& systemId,
        const std::vector<api::Attribute>& attributes,
        std::function<void(api::ResultCode, std::vector<api::Attribute>)> completionHandler) override;

    virtual void addSystemAttribute(
        const std::string& systemId,
        const api::Attribute& attribute,
        std::function<void(api::ResultCode, api::Attribute)> completionHandler) override;

    virtual void updateSystemAttribute(
        const std::string& systemId,
        const api::Attribute& attribute,
        std::function<void(api::ResultCode, api::Attribute)> completionHandler) override;

    virtual void getSystemAttributes(
        const std::string& systemId,
        std::function<void(api::ResultCode, std::vector<api::Attribute>)> completionHandler) override;

    virtual void deleteSystemAttribute(
        const std::string& systemId,
        const std::string& attrName,
        std::function<void(api::ResultCode)> completionHandler) override;

    virtual void updateUserAttributes(
        const std::string& systemId,
        const std::string& email,
        const std::vector<api::Attribute>& attributes,
        std::function<void(api::ResultCode, std::vector<api::Attribute>)> completionHandler) override;

    virtual void updateUserAttribute(
        const std::string& systemId,
        const std::string& email,
        const api::Attribute& attribute,
        std::function<void(api::ResultCode, api::Attribute)> completionHandler) override;

    virtual void getUserAttributes(
        const std::string& systemId,
        const std::string& email,
        std::function<void(api::ResultCode, std::vector<api::Attribute>)> completionHandler) override;

    virtual void deleteUserAttribute(
        const std::string& systemId,
        const std::string& email,
        const std::string& attrName,
        std::function<void(api::ResultCode)> completionHandler) override;

private:
    AsyncRequestsExecutor* m_requestsExecutor = nullptr;
};

} // namespace nx::cloud::db::client
