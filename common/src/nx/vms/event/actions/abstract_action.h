#pragma once

#include <QtCore/QSharedPointer>
#include <QtCore/QVector>

#include <utils/common/id.h>
#include <core/resource/resource_fwd.h>

#include <nx/vms/event/event_fwd.h>
#include <nx/vms/event/action_parameters.h>
#include <nx/vms/event/event_parameters.h>
#include <nx/fusion/model_functions_fwd.h>

namespace nx {
namespace vms {
namespace event {

// TODO: #GDM #Business fix to resourceTypeRequired: None, Camera, Server, User, etc
bool requiresCameraResource(ActionType actionType);
bool requiresUserResource(ActionType actionType);
bool requiresAdditionalUserResource(ActionType actionType);

bool hasToggleState(ActionType actionType);
bool canBeInstant(ActionType actionType);
bool supportsDuration(ActionType actionType);
bool allowsAggregation(ActionType actionType);

bool isActionProlonged(ActionType actionType, const ActionParameters &parameters);

QList<ActionType> userAvailableActions();
QList<ActionType> allActions();

struct ActionData
{
    // TODO: #EC2 Add comments. Maybe remove the flag altogether. What is it for? Which actions?
    enum Flags {
        VideoLinkExists = 1
    };

    ActionData(): actionType(undefinedAction), flags(0) {}
    ActionData(const ActionData&);
    ActionData(ActionData&&) = default;
    ActionData& operator=(const ActionData&) = delete;
    ActionData& operator=(ActionData&&) = default;

    bool hasFlags(int value) const { return flags & value; }

    ActionType actionType;
    ActionParameters actionParams;
    EventParameters eventParams;
    QnUuid businessRuleId; //< TODO: Should be renamed to eventRuleId, considering compatibility.
    int aggregationCount;

    int flags;
    QString compareString; //< TODO: This string is used on a client side for internal purpose. Need to move it to separate class.
};

#define ActionData_Fields (actionType)(actionParams)(eventParams)(businessRuleId)(aggregationCount)(flags)

QN_FUSION_DECLARE_FUNCTIONS(ActionData, (ubjson)(json)(xml)(csv_record));

/**
 * Base class for business actions
 */
class AbstractAction
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
    QVector<QnUuid> getSourceResources() const;

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

} // namespace event
} // namespace vms
} // namespace nx

Q_DECLARE_METATYPE(nx::vms::event::AbstractActionPtr)
Q_DECLARE_METATYPE(nx::vms::event::AbstractActionList)
