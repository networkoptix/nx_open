/**********************************************************
* 20 apr 2015
* a.kolesnikov
***********************************************************/

#ifndef AVIGILON_RESOURCE_H
#define AVIGILON_RESOURCE_H

#ifdef ENABLE_ONVIF

#include <vector>

#include <QString>

#include <plugins/resource/onvif/onvif_resource.h>
#include <nx/network/deprecated/asynchttpclient.h>


/*!
    Uses Avigilon native API to get input port state from camera, since it is broken in Avigilon onvif implementation
*/
class QnAvigilonResource
:
    public QnPlOnvifResource
{
    Q_OBJECT

public:
    QnAvigilonResource();
    virtual ~QnAvigilonResource();

protected:
    virtual bool startInputPortMonitoringAsync( std::function<void(bool)>&& completionHandler ) override;
    virtual void stopInputPortMonitoringAsync() override;
    virtual bool isInputPortMonitored() const override;

private:
    nx::network::http::AsyncHttpClientPtr m_checkInputPortsRequest;
    mutable QnMutex m_ioPortMutex;
    nx::utils::Url m_checkInputUrl;
    bool m_inputMonitored;
    qint64 m_checkInputPortStatusTimerID;
    //!true - closed, falswe - opened
    std::vector<bool> m_relayInputStates;

    void checkInputPortState( qint64 timerID );

private slots:
    void onCheckPortRequestDone( nx::network::http::AsyncHttpClientPtr httpClient );
};

#endif  //ENABLE_ONVIF

#endif  //AVIGILON_RESOURCE_H
