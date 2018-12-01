/**********************************************************
* 12 apr 2013
* akolesnikov
***********************************************************/

#ifndef AXIS_RELAYIO_MANAGER_H
#define AXIS_RELAYIO_MANAGER_H

#include <list>
#include <map>
#include <vector>

#include <QtCore/QAtomicInt>
#include <QtCore/QObject>
#include <QtCore/QMutex>
#include <QtCore/QWaitCondition>
#include <QtCore/QTimer>
#include <QtNetwork/QNetworkReply>

#include <camera/camera_plugin.h>
#include <plugins/plugin_tools.h>


class CameraManager;

//!Provides access to camera's relay input/output port (implements \a nxcip::CameraRelayIOManager)
/*!
    \note Holds reference to \a CameraManager
    \note Delegates reference counting to \a CameraManager instance (i.e., increments \a CameraManager reference counter on initialiation and decrements on destruction)
*/
class RelayIOManager
:
    public QObject,
    public nxcip::CameraRelayIOManager
{
    Q_OBJECT

public:
    /*!
        \note Works in \a CameraPlugin::instance()->networkAccessManager() thread
    */
    RelayIOManager(
        CameraManager* cameraManager,
        const std::vector<QByteArray>& relayParamsStr );
    virtual ~RelayIOManager();

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
    CameraManager* m_cameraManager;
    unsigned int m_inputPortCount;
    unsigned int m_outputPortCount;
    std::map<QString, unsigned int> m_inputPortNameToIndex;
    std::map<QString, unsigned int> m_outputPortNameToIndex;
    std::list<nxcip::CameraInputEventHandler*> m_eventHandlers;
    //!For synchronizing access to event handler list
    mutable QMutex m_mutex;
    QWaitCondition m_cond;
    QAtomicInt m_asyncCallCounter;
    //!map<asyncCallID, callContext>
    std::map<int, AsyncCallContext> m_awaitedAsyncCallIDs;
    MultipartedParsingState m_multipartedParsingState;
    QTimer m_inputCheckTimer;
    //!map<port index starting with 1, port state>
    std::map<int, int> m_inputPortState;

    void copyPortList(
        char** idList,
        int* idNum,
        const std::map<QString, unsigned int>& portNameToIndex ) const;
    void callSlotFromOwningThread( const char* slotName, int* const resultCode = NULL );

private slots:
    void onTimer();
};

#endif  //AXIS_RELAYIO_MANAGER_H
