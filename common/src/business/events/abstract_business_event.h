#ifndef __ABSTRACT_BUSINESS_EVENT_H_
#define __ABSTRACT_BUSINESS_EVENT_H_

#include <QByteArray>
#include <QSharedPointer>
#include "core/resource/resource_fwd.h"
#include <business/business_logic_common.h>

namespace BusinessEventType
{
    enum Value
    {
        /** Motion has occured on a camera. */
        Camera_Motion,

        /** Camera was disconnected. */
        Camera_Disconnect,

        /** Storage read error has occured. */
        Storage_Failure,

        /** Network issue: packet lost, RTP timeout, etc. */
        Network_Issue,

        /** Found some cameras with same IP address. */
        Camera_Ip_Conflict,

        /** Camera input signal is received. */
        Camera_Input,

        /** Connection to mediaserver lost. */
        MediaServer_Failure,

        /** Two or more mediaservers are running. */
        MediaServer_Conflict,

        /** Event type is not defined. Used in rules. */
        NotDefined,

        /**
         * Used when enumerating to build GUI lists, this and followed actions
         * should not be displayed.
         */
        Count = NotDefined,

        /** System health message. */
        SystemHealthMessage = 500,

        /** Base index for the user defined events. */
        UserDefined = 1000

    };

    QString toString( Value val );
    QString toString( Value val, const QString &resourceName);

    bool isResourceRequired(Value val);

    bool hasToggleState(Value val);

    bool requiresCameraResource(Value val);

    bool requiresServerResource(Value val);
}

namespace QnBusinessEventRuntime {
    BusinessEventType::Value getEventType(const QnBusinessParams &params);
    void setEventType(QnBusinessParams* params, BusinessEventType::Value value);

    qint64 getEventTimestamp(const QnBusinessParams &params);
    void setEventTimestamp(QnBusinessParams* params, qint64 value);

    int getEventResourceId(const QnBusinessParams &params);
    void setEventResourceId(QnBusinessParams* params, int value);

}

/**
 * @brief The QnAbstractBusinessEvent class
 *                              Base class for business events. Contains parameters of the
 *                              occured event and methods for checking it against the rules.
 *                              No classes should directly inherit QnAbstractBusinessEvent
 *                              except the QnInstantBusinessEvent and QnProlongedBusinessEvent.
 */
class QnAbstractBusinessEvent
{
protected:
    /**
     * @brief QnAbstractBusinessEvent
     *                          Explicit constructor that MUST be overridden in descendants.
     * @param eventType         Type of the event.
     * @param resource          Resources that provided the event.
     * @param toggleState       On/off state of the event if it is toggleable.
     * @param timeStamp         Event date and time in usec from UTC.
     */
    QnAbstractBusinessEvent(BusinessEventType::Value eventType, const QnResourcePtr& resource, ToggleState::Value toggleState, qint64 timeStamp);

public:
    virtual ~QnAbstractBusinessEvent();

    /**
     * @brief getResource       Get resource that provided this event.
     * @return                  Shared pointer on the resource.
     */
    const QnResourcePtr& getResource() const { return m_resource; }

    /**
     * @brief getEventType      Get type of event. See BusinessEventType::Value.
     * @return                  Enumeration value on the event.
     */
    BusinessEventType::Value getEventType() const { return m_eventType; }

    /**
     * @brief getToggleState    Get on/off state of the event.
     * @return                  Enumeration value. See ToggleState::Value.
     */
    ToggleState::Value getToggleState()     const { return m_toggleState; }

    /**
     * @brief checkCondition    Checks event parameters. Default implementation includes
     *                          check agains ToggleState only.
     * @param params            Parameters of an event that are selected in rule.
     * @return                  True if event should be handled, false otherwise.
     */
    virtual bool checkCondition (ToggleState::Value state, const QnBusinessParams& params) const = 0;

    virtual QnBusinessParams getRuntimeParams() const;

private:
    /**
     * @brief m_eventType       Type of event. See BusinessEventType::Value.
     */
    const BusinessEventType::Value m_eventType;

    /**
     * @brief m_timeStamp       Event date and time in usec from UTC.
     */
    const qint64 m_timeStamp;

    /**
     * @brief m_resource        Resource that provide this event.
     */
    const QnResourcePtr m_resource;

    /**
     * @brief m_toggleState     State on/off for togglable events.
     */
    const ToggleState::Value m_toggleState;
};

typedef QSharedPointer<QnAbstractBusinessEvent> QnAbstractBusinessEventPtr;

Q_DECLARE_METATYPE(BusinessEventType::Value)
Q_DECLARE_METATYPE(QnAbstractBusinessEventPtr)


#endif // __ABSTRACT_BUSINESS_EVENT_H_
