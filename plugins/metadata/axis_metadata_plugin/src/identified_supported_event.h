#pragma once

#include "camera_controller.h"

#include <QtCore/QString>
#include <QtCore/QFlag>

#include <boost/optional/optional.hpp>

#include <plugins/plugin_api.h>
#include <plugins/plugin_tools.h>
#include <nx/api/analytics/driver_manifest.h>
#include <nx/fusion/model_functions_fwd.h>

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace axis {

class IdentifiedSupportedEvent: private nx::axis::SupportedEvent
{
    QnUuid m_internalTypeId;
    nxpl::NX_GUID m_externalTypeId;
public:
    IdentifiedSupportedEvent(const nx::axis::SupportedEvent& supportedEvent);
    const nx::axis::SupportedEvent& base() const { return *this; }
    const QnUuid& internalTypeId() const { return m_internalTypeId; }
    const nxpl::NX_GUID& externalTypeId() const { return m_externalTypeId; }
};

QString serializeEvent(const IdentifiedSupportedEvent& identifiedSupportedEvent);
QString serializeEvents(const QList<IdentifiedSupportedEvent>& identifiedSupportedEvents);

} // axis
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx
