#include "error_bus_message.h"

namespace nx {
namespace gstreamer {

Error ErrorBusMessage::errorMessage() const
{
    gchar* debugInfo;
    GError* nativeError;

    gst_message_parse_error(m_message.get(), &nativeError, &debugInfo);

    Error error;
    error.message = nativeError->message;

    g_free(debugInfo);
    g_error_free(nativeError);

    return error;
}

} // namespace gstreamer
} // namespace nx
