// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../event_list_model.h"

#include <QtCore/QList>

#include <core/resource/resource_fwd.h>

#include <nx/utils/data_structures/keyed_list.h>

namespace nx::vms::client::desktop {

class EventListModel::Private: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    explicit Private(EventListModel* q);
    virtual ~Private() override;

    int count() const;

    void clear();

    bool addFront(const EventData& data);
    bool addBack(const EventData& data);

    bool removeEvent(const QnUuid& id);
    void removeEvents(int first, int count);

    // Event index lookup by id. Logarithmic complexity.
    int indexOf(const QnUuid& id) const;

    bool updateEvent(const EventData& data);
    bool updateEvent(QnUuid id);

    const EventData& getEvent(int index) const;

    QnVirtualCameraResourcePtr previewCamera(const EventData& event) const;
    QnVirtualCameraResourceList accessibleCameras(const EventData& event) const;

private:
    EventListModel* const q = nullptr;
    utils::KeyedList<QnUuid, EventData> m_events;
};

} // namespace nx::vms::client::desktop
