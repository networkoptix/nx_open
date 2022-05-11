// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>
#include <string>

#include "result_code.h"
#include "system_data.h"

namespace nx::cloud::db::api {

/**
 * Provides access to functionality related to cloud systems.
 */
class SystemManager
{
public:
    virtual ~SystemManager() = default;

    /**
     * Binds system to an account associated with authzInfo.
     * @note Required access role: account.
     */
    virtual void bindSystem(
        SystemRegistrationData registrationData,
        std::function<void(ResultCode, SystemData)> completionHandler) = 0;

    /**
     * Unbind system from account.
     * This method MUST be called before binding system to another account.
     * @note Required access role: account (owner).
     */
    virtual void unbindSystem(
        const std::string& systemId,
        std::function<void(ResultCode)> completionHandler) = 0;

    /**
     * Fetch all systems, allowed for current credentials.
     * E.g., for system credentials, only one system is returned.
     * For account credentials systems of account are returned.
     * @note Required access role: account, cloud_db module (e.g., connection_mediator).
     */
    virtual void getSystems(
        std::function<void(ResultCode, api::SystemDataExList)> completionHandler) = 0;

    virtual void getSystemsFiltered(
        const api::Filter& filter,
        std::function<void(ResultCode, api::SystemDataExList)> completionHandler) = 0;

    /**
     * Get system by id.
     */
    virtual void getSystem(
        const std::string& systemId,
        std::function<void(ResultCode, api::SystemDataEx)> completionHandler) = 0;

    virtual void getSystems(
        SystemIdList systemIdList,
        std::function<void(ResultCode, api::SystemDataExList)> completionHandler) = 0;

    /**
     * Share system with specified account. Operation allowed for system owner and editor_with_sharing only.
     * @note Required access role: account (owner or editor_with_sharing).
     * @note sharing is removed if sharingData.accessRole is api::SystemAccessRole::none.
     */
    virtual void shareSystem(
        SystemSharing sharingData,
        std::function<void(ResultCode, SystemSharing)> completionHandler) = 0;

    /**
     * Revokes user with given email from accessing system with the specified systemId.
     */
    virtual void revokeUserAccess(
        const std::string& systemId,
        const std::string& email,
        std::function<void(ResultCode)> completionHandler) = 0;

    /**
     * Returns sharings (account email, access role) for the specified system.
     * @note owner or cloudAdmin account credentials MUST be provided.
     */
    virtual void getCloudUsersOfSystem(
        const std::string& systemId,
        std::function<void(api::ResultCode, api::SystemSharingExList)> completionHandler) = 0;

    /**
     * Returns list of access roles which can be used to share system systemId.
     * @note request is authorized with account credentials.
     */
    virtual void getAccessRoleList(
        const std::string& systemId,
        std::function<void(api::ResultCode, api::SystemAccessRoleList)> completionHandler) = 0;

    /**
     * @note Currently, request can be performed by system only, not by account.
     */
    virtual void update(
        const std::string& systemId,
        const SystemAttributesUpdate& updatedData,
        std::function<void(api::ResultCode)> completionHandler) = 0;

    /**
     * @note Currently, request can be performed by system only, not by account.
     */
    virtual void rename(
        const std::string& systemId,
        const std::string& systemName,
        std::function<void(api::ResultCode)> completionHandler) = 0;

    /**
     * This method MUST be authorized with account credentials.
     */
    virtual void recordUserSessionStart(
        const std::string& systemId,
        std::function<void(api::ResultCode)> completionHandler) = 0;

    virtual void getSystemHealthHistory(
        const std::string& systemId,
        std::function<void(api::ResultCode, api::SystemHealthHistory)> completionHandler) = 0;

    /**
     * Result of merge is system idOfSystemToMergeToId.
     */
    virtual void startMerge(
        const std::string& idOfSystemToMergeTo,
        const std::string& idOfSystemBeingMerged,
        std::function<void(api::ResultCode)> completionHandler) = 0;

    /**
     * Merge systems 5.0+.
     */
    virtual void startMerge(
        const std::string& idOfSystemToMergeTo,
        const MergeRequest& request,
        std::function<void(api::ResultCode)> completionHandler) = 0;

    virtual void validateMSSignature(
        const std::string& systemId,
        const api::ValidateMSSignatureRequest& request,
        std::function<void(api::ResultCode)> completionHandler) = 0;

    //---------------------------------------------------------------------------------------------
    // System ownership transfer methods.

    virtual void offerSystem(
        api::CreateSystemOfferRequest request,
        std::function<void(api::ResultCode)> completionHandler) = 0;

    virtual void acceptSystemOffer(
        const std::string& systemId,
        std::function<void(api::ResultCode)> completionHandler) = 0;

    virtual void rejectSystemOffer(
        const std::string& systemId,
        std::function<void(api::ResultCode)> completionHandler) = 0;

    virtual void deleteSystemOffer(
        const std::string& systemId,
        std::function<void(api::ResultCode)> completionHandler) = 0;

    virtual void getSystemOffers(
        std::function<void(api::ResultCode, std::vector<api::SystemOffer>)> completionHandler) = 0;
};

} // namespace nx::cloud::db::api
