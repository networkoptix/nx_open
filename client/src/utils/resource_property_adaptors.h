#ifndef QN_RESOURCE_PROPERTY_ADAPTORS_H
#define QN_RESOURCE_PROPERTY_ADAPTORS_H

#include <api/resource_property_adaptor.h>

#include <client/client_model_types.h>

#include <business/business_fwd.h>

class QnBusinessEventsFilterResourcePropertyAdaptor: public QnLexicalResourcePropertyAdaptor<quint64> {
    Q_OBJECT
    typedef QnLexicalResourcePropertyAdaptor<quint64> base_type;

public:
    QnBusinessEventsFilterResourcePropertyAdaptor(QObject *parent = NULL):
        base_type(lit("showBusinessEvents"), 0xFFFFFFFFFFFFFFFFull, parent)
    {}

    bool isAllowed(QnBusiness::EventType eventType) const {
        return value() & (1ull << eventType);
    }
};


class QnPtzHotkeysResourcePropertyAdaptor: public QnJsonResourcePropertyAdaptor<QnPtzHotkeyHash> {
    Q_OBJECT
    typedef QnJsonResourcePropertyAdaptor<QnPtzHotkeyHash> base_type;

public:
    QnPtzHotkeysResourcePropertyAdaptor(QObject *parent = NULL):
        base_type(lit("ptzHotkeys"), QnPtzHotkeyHash(), parent)
    {}
};


#endif // QN_RESOURCE_PROPERTY_ADAPTORS_H
