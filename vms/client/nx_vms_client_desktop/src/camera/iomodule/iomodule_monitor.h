// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <api/model/api_ioport_data.h>
#include <core/resource/resource_fwd.h>
#include <nx/network/deprecated/asynchttpclient.h>
#include <nx/network/http/multipart_content_parser.h>
#include <nx/utils/thread/mutex.h>
#include <nx/vms/client/core/common/utils/common_module_aware.h>
#include <nx/vms/client/core/network/remote_connection_aware.h>

class QnIOModuleMonitor:
    public QObject,
    public nx::vms::client::core::CommonModuleAware,
    public nx::vms::client::core::RemoteConnectionAware
{
    Q_OBJECT
public:
    QnIOModuleMonitor(const QnSecurityCamResourcePtr &camera);
    virtual ~QnIOModuleMonitor() override;
    bool open();
signals:
    void connectionClosed();
    void connectionOpened();
    void ioStateChanged(const QnIOStateData& value);
private slots:
    void at_MonitorResponseReceived( nx::network::http::AsyncHttpClientPtr httpClient );
    void at_MonitorMessageBodyAvailable( nx::network::http::AsyncHttpClientPtr httpClient );
    void at_MonitorConnectionClosed( nx::network::http::AsyncHttpClientPtr httpClient );
private:
    mutable nx::Mutex m_mutex;
    QnSecurityCamResourcePtr m_camera;
    nx::network::http::AsyncHttpClientPtr m_httpClient;
    std::shared_ptr<nx::network::http::MultipartContentParser> m_multipartContentParser;
};

typedef QSharedPointer<QnIOModuleMonitor> QnIOModuleMonitorPtr;
