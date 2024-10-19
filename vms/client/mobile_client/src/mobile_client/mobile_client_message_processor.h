// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <client/client_message_processor.h>
#include <core/resource/resource_fwd.h>

class QnMobileClientMessageProcessor: public QnClientMessageProcessor
{
    Q_OBJECT
    typedef QnClientMessageProcessor base_type;

public:
    using QnClientMessageProcessor::QnClientMessageProcessor;

    virtual void updateResource(const QnResourcePtr &resource, ec2::NotificationSource source) override;

protected:
    virtual QnResourceFactory* getResourceFactory() const override;

private:
    void updateMainServerApiUrl(const QnMediaServerResourcePtr& server);
};

#define qnMobileClientMessageProcessor \
    static_cast<QnMobileClientMessageProcessor*>(this->messageProcessor())
