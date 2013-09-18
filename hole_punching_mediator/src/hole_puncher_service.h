/**********************************************************
* 27 aug 2013
* a.kolesnikov
***********************************************************/

#ifndef HOLE_PUNCHER_SERVICE_H
#define HOLE_PUNCHER_SERVICE_H

#include <memory>

#include <QtCore/QSettings>

#include <qtsinglecoreapplication.h>
#include <qtservice.h>

#include <utils/common/stoppable.h>

#include "hole_punching_requests_processor.h"
#include "stream_socket_server.h"


class StreamSocketServer;

class HolePuncherProcess
:
    public QtService<QtSingleCoreApplication>,
    public QnStoppable
{
public:
    HolePuncherProcess( int argc, char **argv );

    //!Implementation of QnStoppable::pleaseStop
    virtual void pleaseStop() override;

protected:
    virtual int executeApplication() override;
    virtual void start() override;
    virtual void stop() override;

private:
    std::unique_ptr<QSettings> m_settings;
    int m_argc;
    char** m_argv;
    std::unique_ptr<HolePunchingRequestsProcessor> m_requestsProcessor;
    std::list<std::unique_ptr<StreamSocketServer> > m_listeners;

    bool initialize();
    void deinitialize();
    QString getDataDirectory();
};

#endif  //HOLE_PUNCHER_SERVICE_H
