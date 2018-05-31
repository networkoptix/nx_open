/**********************************************************
* 12 apr 2013
* akolesnikov
***********************************************************/

#ifndef AXIS_RELAYIO_MANAGER_H
#define AXIS_RELAYIO_MANAGER_H

#include <list>
#include <map>
#include <set>

#include <QtCore/QAtomicInt>
#include <QtCore/QObject>
#include <QtCore/QMutex>
#include <QtNetwork/QNetworkReply>
#include <QtCore/QWaitCondition>

#include <camera/camera_plugin.h>
#include <plugins/plugin_tools.h>


class AxisCameraManager;

//!Provides access to camera's relay input/output port (implements \a nxcip::CameraRelayIOManager)
/*!
    \note Holds reference to \a AxisCameraManager
    \note Delegates reference counting to \a AxisCameraManager instance (i.e., increments \a AxisCameraManager reference counter on initialiation and decrements on destruction)
*/
class AxisRelayIOManager
:
    public QObject,
    public nxcip::CameraRelayIOManager
{
    Q_OBJECT

public:
    /*!
        \note Works in \a AxisCameraPlugin::instance()->networkAccessManager() thread
    */
    AxisRelayIOManager(
        AxisCameraManager* cameraManager,
        unsigned int inputPortCount,
        unsigned int outputPortCount );
    virtual ~AxisRelayIOManager();

    //!Implementation of nxcip::CameraRelayIOManager::queryInterface
    virtual void* queryInterface( const nxpl::NX_GUID& interfaceID ) override;
    //!Implementaion of nxpl::PluginInterface::addRef
    virtual unsigned int addRef() override;
    //!Implementaion of nxpl::PluginInterface::releaseRef
    virtual unsigned int releaseRef() override;

    //!Implementation of nxcip::CameraRelayIOManager::getRelayOutputList
    virtual int getRelayOutputList( char** idList, int* idNum ) const override;
    //!Implementation of nxcip::CameraRelayIOManager::getRelayOutputList
    virtual int getInputPortList( char** idList, int* idNum ) const override;
    //!Implementation of nxcip::CameraRelayIOManager::getRelayOutputList
    virtual int setRelayOutputState(
        const char* outputID,
        int activate,
        unsigned int autoResetTimeoutMS ) override;
    //!Implementation of nxcip::CameraRelayIOManager::getRelayOutputList
    virtual int startInputPortMonitoring() override;
    //!Implementation of nxcip::CameraRelayIOManager::getRelayOutputList
    virtual void stopInputPortMonitoring() override;
    //!Implementation of nxcip::CameraRelayIOManager::getRelayOutputList
    virtual void registerEventHandler( nxcip::CameraInputEventHandler* handler ) override;
    //!Implementation of nxcip::CameraRelayIOManager::getRelayOutputList
    virtual void unregisterEventHandler( nxcip::CameraInputEventHandler* handler ) override;
    //!Implementation of nxcip::CameraRelayIOManager::getRelayOutputList
    virtual void getLastErrorString( char* errorString ) const override;

private:
    struct AsyncCallContext
    {
        AsyncCallContext()
        :
            done( false ),
            resultCode( 0 )
        {
        }

        bool done;
        int resultCode;
    };

    enum MultipartedParsingState
    {
        waitingDelimiter,
        readingHeaders,
        readingData
    };

    nxpt::CommonRefManager m_refManager;
    AxisCameraManager* m_cameraManager;
    unsigned int m_inputPortCount;
    unsigned int m_outputPortCount;
    std::map<QString, unsigned int> m_inputPortNameToIndex;
    std::map<QString, unsigned int> m_outputPortNameToIndex;
    std::list<nxcip::CameraInputEventHandler*> m_eventHandlers;
    //!For synchronizing access to event handler list
    mutable QMutex m_mutex;
    std::map<unsigned int, QNetworkReply*> m_inputPortHttpMonitor;
    QWaitCondition m_cond;
    QAtomicInt m_asyncCallCounter;
    //!map<asyncCallID, callContext>
    std::map<int, AsyncCallContext> m_awaitedAsyncCallIDs;
    MultipartedParsingState m_multipartedParsingState;

    void copyPortList(
        char** idList,
        int* idNum,
        const std::map<QString, unsigned int>& portNameToIndex ) const;
    void callSlotFromOwningThread( const char* slotName, int* const resultCode = NULL );
    void readAxisRelayPortNotification( const QByteArray& line );

private slots:
    void startInputPortMonitoringPriv( int asyncCallID );
    void stopInputPortMonitoringPriv( int asyncCallID );
    void onMonitorDataAvailable();
    void onConnectionFinished( QNetworkReply* reply );
};

#endif  //AXIS_RELAYIO_MANAGER_H
