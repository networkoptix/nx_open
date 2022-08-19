// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <client/client_message_processor.h>

class NX_VMS_CLIENT_DESKTOP_API QnDesktopClientMessageProcessor: public QnClientMessageProcessor
{
    Q_OBJECT
    using base_type = QnClientMessageProcessor;

public:
    using QnClientMessageProcessor::QnClientMessageProcessor;

protected:
    virtual QnResourceFactory* getResourceFactory() const override;
    virtual void updateResource(
        const QnResourcePtr& resource,
        ec2::NotificationSource source) override;
};

#define qnDesktopClientMessageProcessor \
    static_cast<QnDesktopClientMessageProcessor*>(commonModule()->messageProcessor())
