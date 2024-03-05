// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/log/log_writers.h>

namespace nx::network::http { class WritableMessageBody; }

namespace nx::network::maintenance::log {

class NX_NETWORK_API StreamingLogWriter:
    public nx::log::AbstractWriter
{
public:
    StreamingLogWriter(nx::network::http::WritableMessageBody* mediator);

    virtual void write(nx::log::Level level, const QString& message) override;

    void setMessageBody(nx::network::http::WritableMessageBody* messageBody);

private:
    nx::Mutex m_mutex;
    nx::network::http::WritableMessageBody* m_messageBody = nullptr;
};

} // namespace nx::network::maintenance::log
