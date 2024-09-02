// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QList>

#include <core/resource/resource_fwd.h>
#include <nx/utils/data_structures/keyed_list.h>

#include "../event_list_model.h"

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
    std::list<EventData> addFront(std::list<EventData> events);
    bool addBack(const EventData& data);
    std::list<EventData> addBack(std::list<EventData> events);

    bool removeEvent(const nx::Uuid& id);
    void removeEvents(int first, int count);

    // Event index lookup by id. Logarithmic complexity.
    int indexOf(const nx::Uuid& id) const;

    bool updateEvent(const EventData& data);
    bool updateEvent(nx::Uuid id);

    const EventData& getEvent(int index) const;

    QnVirtualCameraResourcePtr previewCamera(const EventData& event) const;
    QnVirtualCameraResourceList accessibleCameras(const EventData& event) const;

private:
    EventListModel* const q = nullptr;
    utils::KeyedList<nx::Uuid, EventData> m_events;

    void eraseExistingEvents(std::list<EventData>& events) const;
};

} // namespace nx::vms::client::desktop
