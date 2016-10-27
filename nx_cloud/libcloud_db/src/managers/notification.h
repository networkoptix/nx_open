#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QString>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/fusion/serialization/json.h>

namespace nx {
namespace cdb {

//-------------------------------------------------------------------------------------------------
// Notification data

class ActivateAccountData
{
public:
    std::string code;
};
#define ActivateAccountData_Fields (code)

class SystemSharedData
{
public:
    std::string sharer_email;
    std::string sharer_name;
    std::string system_name;
    std::string system_id;
};
#define SystemSharedData_Fields (sharer_email)(sharer_name)(system_name)(system_id)

class InviteUserData:
    public ActivateAccountData,
    public SystemSharedData
{
};
#define InviteUserData_Fields \
    ActivateAccountData_Fields \
    SystemSharedData_Fields

template<class MessageData>
class NotificationData
{
public:
    std::string user_email;
    std::string type;
    MessageData message;
    std::string secret;
};
#define NotificationData_Fields (user_email)(type)(message)(secret)

typedef NotificationData<ActivateAccountData> ActivateAccountNotificationData;
#define ActivateAccountNotificationData_Fields NotificationData_Fields

typedef NotificationData<SystemSharedData> SystemSharedNotificationData;
#define SystemSharedNotificationData_Fields NotificationData_Fields

typedef NotificationData<InviteUserData> InviteUserNotificationData;
#define InviteUserNotificationData_Fields NotificationData_Fields

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (ActivateAccountData) \
        (SystemSharedData) \
        (InviteUserData) \
        (ActivateAccountNotificationData) \
        (InviteUserNotificationData) \
        (SystemSharedNotificationData),
    (json))

//-------------------------------------------------------------------------------------------------
// Notification implementation helpers

class AbstractNotification
{
public:
    virtual ~AbstractNotification() = default;

    virtual void setAddressee(std::string addressee) = 0;
    virtual QByteArray serializeToJson() const = 0;
};

class AbstractActivateAccountNotification:
    public AbstractNotification
{
public:
    virtual void setActivationCode(std::string code) = 0;
};

/**
 * Provides \a AbstractNotification::serializeToJson implementation for a descendant.
 */
template<typename BaseType, typename FusionEnabledDescendantType>
class NotificationSerializationProvider:
    public BaseType
{
public:
    virtual void setAddressee(std::string addressee) override
    {
        static_cast<FusionEnabledDescendantType*>(this)->user_email
            = std::move(addressee);
    }

    virtual QByteArray serializeToJson() const override
    {
        return QJson::serialized<FusionEnabledDescendantType>(
            *static_cast<const FusionEnabledDescendantType*>(this));
    }
};

template<typename BaseType, typename FusionEnabledDescendantType>
class AbstractActivateAccountNotificationImplementor:
    public NotificationSerializationProvider<BaseType, FusionEnabledDescendantType>
{
public:
    virtual void setActivationCode(std::string code) override
    {
        static_cast<FusionEnabledDescendantType*>(this)->message.code
            = std::move(code);
    }
};

//-------------------------------------------------------------------------------------------------
// Notifications

class ActivateAccountNotification:
    public AbstractActivateAccountNotificationImplementor<
        AbstractActivateAccountNotification, ActivateAccountNotification>,
    public ActivateAccountNotificationData
{
public:
    ActivateAccountNotification();
};

class RestorePasswordNotification:
    public AbstractActivateAccountNotificationImplementor<
        AbstractActivateAccountNotification, RestorePasswordNotification>,
    public ActivateAccountNotificationData
{
public:
    RestorePasswordNotification();
};

class SystemSharedNotification:
    public NotificationSerializationProvider<
        AbstractNotification, SystemSharedNotification>,
    public SystemSharedNotificationData
{
public:
    SystemSharedNotification();
};

class InviteUserNotification:
    public AbstractActivateAccountNotificationImplementor<
        AbstractActivateAccountNotification, InviteUserNotification>,
    public InviteUserNotificationData
{
public:
    InviteUserNotification();
};

} // namespace cdb
} // namespace nx
