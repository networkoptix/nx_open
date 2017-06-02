#pragma once

#include <nx/vms/event/actions/abstract_action.h>

class QnEventSerializer
{
public:
    static void deserialize(nx::vms::event::ActionDataListPtr& events, const QByteArray& data);
};
