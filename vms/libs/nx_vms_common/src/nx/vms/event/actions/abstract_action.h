// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QFlags>
#include <QtCore/QSharedPointer>
#include <QtCore/QVector>

#include <core/resource/resource_fwd.h>
#include <nx/fusion/model_functions_fwd.h>
#include <nx/vms/event/action_parameters.h>
#include <nx/vms/event/event_fwd.h>
#include <nx/vms/event/event_parameters.h>
#include <utils/common/id.h>

class QnResourcePool;

namespace nx::vms::event {

enum class ActionFlag {
    canUseSourceCamera = 1 << 0,
};

Q_DECLARE_FLAGS(ActionFlags, ActionFlag)

NX_VMS_COMMON_API ActionFlags getFlags(ActionType actionType);

// TODO: #sivanov Fix to resourceTypeRequired: None, Camera, Server, User, etc.
NX_VMS_COMMON_API bool requiresCameraResource(ActionType actionType);
NX_VMS_COMMON_API bool requiresUserResource(ActionType actionType);
NX_VMS_COMMON_API bool requiresServerResource(ActionType actionType);
NX_VMS_COMMON_API bool requiresAdditionalUserResource(ActionType actionType);

NX_VMS_COMMON_API bool hasToggleState(ActionType actionType);
NX_VMS_COMMON_API bool canBeInstant(ActionType actionType);
NX_VMS_COMMON_API bool supportsDuration(ActionType actionType);
NX_VMS_COMMON_API bool allowsAggregation(ActionType actionType);
NX_VMS_COMMON_API bool canUseSourceCamera(ActionType actionType);

NX_VMS_COMMON_API bool isActionProlonged(ActionType actionType, const ActionParameters &parameters);

NX_VMS_COMMON_API QList<ActionType> userAvailableActions();
NX_VMS_COMMON_API QList<ActionType> allActions();

struct NX_VMS_COMMON_API ActionData
{
    // TODO: #EC2 Add comments. Maybe remove the flag altogether. What is it for? Which actions?
    enum Flags {
        VideoLinkExists = 1
    };

    ActionData(): actionType(ActionType::undefinedAction), flags(0) {}
    ActionData(const ActionData&) = default;
    ActionData(ActionData&&) = default;
    ActionData& operator=(const ActionData&) = default;
    ActionData& operator=(ActionData&&) = default;

    bool hasFlags(int value) const { return flags & value; }

    /**%apidoc Type of the Action. */
    ActionType actionType;

    /**%apidoc
     * Parameters of the Action. Only fields which are applicable to the particular Action are used.
     */
    ActionParameters actionParams;

    /**%apidoc Parameters of the Event which triggered the Action. */
    EventParameters eventParams;

    /**%apidoc Id of the Event Rule. */
    QnUuid businessRuleId; //< TODO: Should be renamed to eventRuleId, considering compatibility.

    /**%apidoc Number of identical Events grouped into one. */
    int aggregationCount;

    /**%apidoc Combination of the flags:
     * - 1 indicates that Video Link exists.
     */
    int flags;
    QString compareString; //< TODO: This string is used on a client side for internal purpose. Need to move it to separate class.

    QString toString() const;
};

#define ActionData_Fields (actionType)(actionParams)(eventParams)(businessRuleId)(aggregationCount)(flags)

QN_FUSION_DECLARE_FUNCTIONS(ActionData, (ubjson)(json)(xml)(csv_record), NX_VMS_COMMON_API)

/**
 * Base class for business actions
 */
class NX_VMS_COMMON_API AbstractAction
{
protected:
    explicit AbstractAction(const ActionType actionType, const EventParameters& runtimeParams);

public:
    virtual ~AbstractAction();

    ActionType actionType() const;

    /**
     * Resource depend of action type.
     * see: requiresCameraResource()
     * see: requiresUserResource()
     */
    void setResources(const QVector<QnUuid>& resources);

    const QVector<QnUuid>& getResources() const;

    /** Source resource of the action (including custom for generic events). */
    QVector<QnUuid> getSourceResources(const QnResourcePool* resourcePool) const;

    void setParams(const ActionParameters& params);
    const ActionParameters& getParams() const;
    ActionParameters& getParams();

    void setRuntimeParams(const EventParameters& params);
    const EventParameters& getRuntimeParams() const;
    EventParameters& getRuntimeParams();

    void setRuleId(const QnUuid& value);
    QnUuid getRuleId() const;

    void setToggleState(EventState value);
    EventState getToggleState() const;

    void setReceivedFromRemoteHost(bool value);
    bool isReceivedFromRemoteHost() const;

    /** How much times action was instantiated during the aggregation period. */
    void setAggregationCount(int value);
    int getAggregationCount() const;

    bool isProlonged() const;

    /** Return action unique key for external outfit (port number for output action e.t.c). Do not count resourceId
     * This function help to share physical resources between actions
     * Do not used for instant actions
     */
    virtual QString getExternalUniqKey() const;

    /** Virtual assignment operation. */
    virtual void assign(const AbstractAction& other);

protected:
    ActionType m_actionType;
    EventState m_toggleState;
    bool m_receivedFromRemoteHost;
    QVector<QnUuid> m_resources;
    ActionParameters m_params;
    EventParameters m_runtimeParams;
    QnUuid m_ruleId; // event rule that generated this action
    int m_aggregationCount;
};

} // namespace nx::vms::event
