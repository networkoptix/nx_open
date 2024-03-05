// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "streaming_log_writer.h"

#include <nx/network/http/writable_message_body.h>

namespace nx::network::maintenance::log {

using namespace nx::network::http;

StreamingLogWriter::StreamingLogWriter(WritableMessageBody* messageBody)
{
    setMessageBody(messageBody);
}

void StreamingLogWriter::write(nx::log::Level /*level*/, const QString& message)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    if (m_messageBody)
        m_messageBody->writeBodyData((message + "\r\n").toStdString());
}

void StreamingLogWriter::setMessageBody(WritableMessageBody * messageBody)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_messageBody = messageBody;
}

} // namespace nx::network::maintenance::log
