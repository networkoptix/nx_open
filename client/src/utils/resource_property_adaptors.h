#ifndef QN_RESOURCE_PROPERTY_ADAPTORS_H
#define QN_RESOURCE_PROPERTY_ADAPTORS_H

#include <api/resource_property_adaptor.h>

#include <client/client_model_types.h>

#include <business/business_fwd.h>

class QnBusinessEventsFilterResourcePropertyAdaptor: public QnLexicalResourcePropertyAdaptor<quint64> {
    Q_OBJECT
    typedef QnLexicalResourcePropertyAdaptor<quint64> base_type;

public:
    QnBusinessEventsFilterResourcePropertyAdaptor(const QnResourcePtr &resource, QObject *parent = NULL):
        base_type(resource, lit("showBusinessEvents"), 0xFFFFFFFFFFFFFFFFull, parent)
    {}

    bool isAllowed(BusinessEventType::Value eventType) const {
        return value() & (1ull << eventType);
    }
};

#endif // QN_RESOURCE_PROPERTY_ADAPTORS_H
