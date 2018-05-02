/**********************************************************
* 20 sep 2013
* a.kolesnikov
***********************************************************/

#ifndef RDIR_SYNCHER_H
#define RDIR_SYNCHER_H

#include <stdint.h>

#include <atomic>
#include <forward_list>
#include <list>
#include <map>
#include <memory>
#include <mutex>

#include <QtCore/QObject>
#include <QtCore/QString>

#include <nx/utils/thread/stoppable.h>
#include <nx/utils/url.h>

#include "rdir_synchronization_operation.h"
#include "list_directory_operation.h"


class RSyncEventTrigger;

//!Synchronizes local directory with remote one using http
/*!
    Remote directory is defined by http url. E.g.: http://x.x.x.x/builds/networkoptix/2.1/. All directory/file names are relative to this directory
    Contents of remote directory is read from contents.xml (url http://x.x.x.x/builds/networkoptix/2.1/contents.xml) file. File has following format:
    \code {*.xml}
    <?xml version="1.0" encoding="UTF-8"?>
    <contents validation="md5">
        <directory>client</directory>
        <directory>mediaserver</directory>
        <directory>appserver</directory>
        <file>help.hlp</file>
    </contents>
    \endcode

    - Each directory (client, mediaserver, appserver) MUST have its own contents.xml

    - optional validation="md5" attribute means that each file CAN be accompanied by *.md5 file (e.g., help.hlp.md5), which contains MD5 hash of file. 
        This information is used to skip downloading duplicate files

    \note Assumes that remote directory contents are static during synchronization
    \note Works in asynchronous mode
    \note Not thread-safe
    \note Can download multiple files simultaneously
    \note At the moment every file is downloaded by single connection
    \note Error in downloading any file result in synchronization interrupt
    \note \a RDirSyncher instance MUST be used by std::shared_ptr
*/
class RDirSyncher
:
    public QnStoppable,
    public detail::AbstractRDirSynchronizationEventHandler,
    public std::enable_shared_from_this<RDirSyncher>
{
public:
    enum State
    {
        //!synchnorization has not been started yet
        sInit,
        sInProgress,
        sSucceeded,
        sInterrupted,
        sFailed
    };

    class EventReceiver
    {
    public:
        virtual ~EventReceiver();

        //!Called just after downloading has been successfully started
        virtual void started( const std::shared_ptr<RDirSyncher>& syncher );

        virtual void overrallDownloadSizeKnown(
            const std::shared_ptr<RDirSyncher>& syncher,
            int64_t totalBytesToDownload );
        virtual void fileStarted(
            const std::shared_ptr<RDirSyncher>& syncher,
            const QString& filePath );
        /*!
            File download progress is not reported by default, but needs to be enabled by \a RDirSyncher::setFileProgressNotificationStep
            \param remoteFileSize -1 if file size if unknown (not reported by http server)
        */
        virtual void fileProgress(
            const std::shared_ptr<RDirSyncher>& syncher,
            const QString& filePath,
            int64_t remoteFileSize,
            int64_t bytesDownloaded );
        /*!
            \param filePath Path relative to \a baseUrl
        */
        virtual void fileDone(
            const std::shared_ptr<RDirSyncher>& syncher,
            const QString& filePath );
        /*!
            \param result \a true, if synchronization succeeded, \a false otherwise
        */
        virtual void finished(
            const std::shared_ptr<RDirSyncher>& syncher,
            bool result );
        /*!
            \param failedFilePath path relative to \a baseUrl
        */
        virtual void failed(
            const std::shared_ptr<RDirSyncher>& syncher,
            const QString& failedFilePath,
            const QString& errorText );
    };

    //!Initialization
    /*!
        \param mirrors these urls are tried in their order
    */
    RDirSyncher(
        const std::forward_list<nx::utils::Url>& mirrors,
        const QString& localDirPath,
        RDirSyncher::EventReceiver* const eventReceiver );
    RDirSyncher(
        const nx::utils::Url& baseUrl,
        const QString& localDirPath,
        RDirSyncher::EventReceiver* const eventReceiver );
    virtual ~RDirSyncher();

    //!Implementation of \a QnStoppable::pleaseStop()
    virtual void pleaseStop() override;

    //!Enable call of \a EventReceiver::fileProgress after downloading each \a bytes
    void setFileProgressNotificationStep( int bytes );
    //!Start download
    /*!
        \param eventReceiver This object receives events about download progress. Events delivered in some unspecified aio thread
        \return \a false if failed to start download. Use \a RDirSyncher::lastErrorText() to get error description
    */
    bool startAsync();
    //!Returns current state
    State state() const;

    void cancel();

    //!Returns description of last error
    QString lastErrorText() const;

    //!Implementation of AbstractRDirSynchronizationEventHandler::downloadProgress
    virtual void downloadProgress(
        const std::shared_ptr<detail::RDirSynchronizationOperation>& operation,
        int64_t remoteFileSize,
        int64_t bytesDownloaded ) override;
    //!Implementation of AbstractRDirSynchronizationEventHandler::operationDone
    virtual void operationDone( const std::shared_ptr<detail::RDirSynchronizationOperation>& operation ) override;

private:
    enum class OperationStartResult
    {
        success,
        nothingToDo,
        failure
    };

    class SynchronizationTask
    :
        public detail::RDirEntry
    {
    public:
        SynchronizationTask( const detail::RDirEntry& _entry )
        :
            RDirEntry( _entry ),
            running( false ),
            retryCount( 0 )
        {
        }

        SynchronizationTask(
            QString _entryPath = QString(),
            detail::RSyncOperationType _type = detail::RSyncOperationType::none,
            QString _hashTypeName = QString() )
        :
            RDirEntry( _entryPath, _type, _hashTypeName ),
            running( false ),
            retryCount( 0 )
        {
        }

        bool running;
        int retryCount;
    };

    std::forward_list<nx::utils::Url> m_mirrors;
    const QString m_localDirPath;
    EventReceiver* m_eventReceiver;
    std::list<SynchronizationTask> m_syncTasks;
    std::mutex m_mutex;
    //!map<id, context>
    std::map<int, std::shared_ptr<detail::RDirSynchronizationOperation>> m_runningOperations;
    std::atomic<int> m_prevOperationID;
    State m_state;
    QString m_lastErrorText;
    QString m_failedEntryPath;
    nx::utils::Url m_currentMirror;

    //!Starts multiple operations by calling \a RDirSyncher::startNextOperation()
    void startOperations( std::list<RSyncEventTrigger*>& eventToTrigger );
    /*!
        \return started operation, or nullptr in case of failure
    */
    OperationStartResult startNextOperation( std::shared_ptr<detail::RDirSynchronizationOperation>* const operation );
    bool processOperation( const std::shared_ptr<detail::RDirSynchronizationOperation>& operation );
    //!Chooses for mirror next url after \a currentMirror
    /*!
        \return true, if switched to another mirror. false, if no mirrors left
    */
    bool useNextMirror( const nx::utils::Url& hintMirror );
};

#endif  //RDIR_SYNCHER_H
