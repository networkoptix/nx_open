#ifndef __ABSTRACT_BUSINESS_EVENT_H_
#define __ABSTRACT_BUSINESS_EVENT_H_

#include <QByteArray>
#include <QSharedPointer>
#include "core/resource/resource_fwd.h"
#include "business_logic_common.h"


namespace BusinessEventType
{
    enum Value
    {
        /** Event type is not defined. Used in rules. */
        BE_NotDefined,

        /** Motion has occured on a camera. */
        BE_Camera_Motion,

        /** Camera input signal is received. */
        BE_Camera_Input,

        /** Camera was disconnected. */
        BE_Camera_Disconnect,

        /** Storage read error has occured. */
        BE_Storage_Failure,

        /** Aliases for the convinient lists building */
        BE_FirstType = BE_Camera_Motion,
        BE_LastType = BE_Storage_Failure,

        /** Base index for the user defined events. */
        BE_UserDefined = 1000
    };

    QString toString( Value val );

    bool isResourceRequired(Value val);

    bool hasToggleState(Value val);

    bool requiresCameraResource(Value val);

    bool requiresServerResource(Value val);
}

namespace BusinessEventParameters {
    ToggleState::Value getToggleState(const QnBusinessParams &params);
    void setToggleState(QnBusinessParams* params, ToggleState::Value value);
}

namespace QnBusinessEventRuntime {
    BusinessEventType::Value getEventType(const QnBusinessParams &params);
    void setEventType(QnBusinessParams* params, BusinessEventType::Value value);

    QString getEventResourceName(const QnBusinessParams &params);
    void setEventResourceName(QnBusinessParams* params, QString value);

    QString getEventResourceUrl(const QnBusinessParams &params);
    void setEventResourceUrl(QnBusinessParams* params, QString value);

    QString getEventDescription(const QnBusinessParams &params);
    void setEventDescription(QnBusinessParams* params, QString value);
}

/**
 * @brief The QnAbstractBusinessEvent class
 *                              Base class for business events. Contains parameters of the
 *                              occured event and methods for checking it against the rules.
 */
class QnAbstractBusinessEvent
{
protected:
    /**
     * @brief QnAbstractBusinessEvent
     *                          Explicit constructor that MUST be overriden in descendants.
     * @param eventType         Type of the event.
     * @param resource          Resources that provided the event.
     * @param toggleState       On/off state of the event if it is toggleable.
     * @param timeStamp         Event date and time in usec from UTC.
     */
    explicit QnAbstractBusinessEvent (
            BusinessEventType::Value eventType,
            QnResourcePtr resource,
            ToggleState::Value toggleState,
            qint64 timeStamp);
public:
    virtual ~QnAbstractBusinessEvent() {}

    /**
     * @brief toString          Convert event to human-readable string in debug purposes and as sendMail text.
     * @return                  Printable string with all event data in human-readable form.
     */
    virtual QString toString() const;

    /**
     * @brief getResource       Get resource that provided this event.
     * @return                  Shared pointer on the resource.
     */
    QnResourcePtr getResource()             const { return m_resource; }

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
    virtual bool checkCondition(const QnBusinessParams& params) const;

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

Q_DECLARE_METATYPE(QnAbstractBusinessEventPtr)


#endif // __ABSTRACT_BUSINESS_EVENT_H_
