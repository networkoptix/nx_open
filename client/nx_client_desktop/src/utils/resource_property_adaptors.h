#ifndef QN_RESOURCE_PROPERTY_ADAPTORS_H
#define QN_RESOURCE_PROPERTY_ADAPTORS_H

#include <api/resource_property_adaptor.h>

#include <client/client_model_types.h>

#include <nx/vms/event/event_fwd.h>

// TODO: #vkutin Think of a better name, location and namespace.
class QnBusinessEventsFilterResourcePropertyAdaptor: public QnLexicalResourcePropertyAdaptor<quint64> {
    Q_OBJECT
    typedef QnLexicalResourcePropertyAdaptor<quint64> base_type;

public:
    QnBusinessEventsFilterResourcePropertyAdaptor(QObject *parent = NULL);

    bool isAllowed(nx::vms::event::EventType eventType) const;

    QList<nx::vms::event::EventType> watchedEvents() const;
    void setWatchedEvents( const QList<nx::vms::event::EventType> &events );
};


class QnPtzHotkeysResourcePropertyAdaptor: public QnJsonResourcePropertyAdaptor<QnPtzHotkeyHash> {
    Q_OBJECT
    typedef QnJsonResourcePropertyAdaptor<QnPtzHotkeyHash> base_type;

public:
    QnPtzHotkeysResourcePropertyAdaptor(QObject *parent = NULL);
};


#endif // QN_RESOURCE_PROPERTY_ADAPTORS_H
