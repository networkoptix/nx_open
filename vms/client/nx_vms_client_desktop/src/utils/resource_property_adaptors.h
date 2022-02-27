// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

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
    QnBusinessEventsFilterResourcePropertyAdaptor(QObject *parent = nullptr);

    bool isAllowed(nx::vms::api::EventType eventType) const;

    QList<nx::vms::api::EventType> watchedEvents() const;
    void setWatchedEvents( const QList<nx::vms::api::EventType> &events );
};

#endif // QN_RESOURCE_PROPERTY_ADAPTORS_H
