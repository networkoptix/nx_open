/**********************************************************
* 27 sep 2013
* a.kolesnikov
***********************************************************/

#ifndef INSTALLATION_PROCESS_H
#define INSTALLATION_PROCESS_H

#include <memory>
#include <mutex>

#include <QObject>
#include <QString>

#include <api/applauncher_api.h>
#include <utils/common/stoppable.h>
#include <utils/common/joinable.h>
#include <utils/network/http/asynchttpclient.h>

#include "rdir_syncher.h"


class QSettings;

/*!
    First, downloads list of mirrors and then downloads requested data
    \note List of mirrors is not saved anywhere but stored in memory only
*/
class InstallationProcess
:
    public QObject,
    public RDirSyncher::EventReceiver,
    public QnStoppable,
    public QnJoinable
{
    Q_OBJECT

public:
    InstallationProcess(
        const QString& productName,
        const QString& customization,
        const QString& version,
        const QString& module,
        const QString& installationDirectory );
    virtual ~InstallationProcess();

    //!Implementation of QnStoppable::pleaseStop()
    virtual void pleaseStop() override;
    //!Implementation of QnJoinable::wait()
    virtual void wait() override;

    bool start( const QSettings& settings );

    applauncher::api::InstallationStatus::Value getStatus() const;
    //!Returns installation progress (percents)
    float getProgress() const;

    QString errorText() const;

private:
    enum class State
    {
        init,
        downloadMirrorList,
        installing,
        finished
    };

    const QString m_productName;
    const QString m_customization;
    const QString m_version;
    const QString m_module;
    const QString m_installationDirectory;
    State m_state;
    nx_http::AsyncHttpClient* m_httpClient;
    applauncher::api::InstallationStatus::Value m_status;
    std::unique_ptr<RDirSyncher> m_syncher;
    QString m_errorText;
    mutable std::mutex m_mutex;

    //!Implementation of RDirSyncher::EventReceiver::fileProgress
    virtual void fileProgress(
        RDirSyncher* const syncher,
        int64_t remoteFileSize,
        int64_t bytesDownloaded ) override;
    //!Implementation of RDirSyncher::EventReceiver::fileProgress
    virtual void fileDone(
        RDirSyncher* const syncher,
        const QString& filePath ) override;
    //!Implementation of RDirSyncher::EventReceiver::fileProgress
    virtual void finished(
        RDirSyncher* const syncher,
        bool result ) override;
    //!Implementation of RDirSyncher::EventReceiver::fileProgress
    virtual void failed(
        RDirSyncher* const syncher,
        const QString& failedFilePath,
        const QString& errorText ) override;

private slots:
    void onHttpDone( nx_http::AsyncHttpClient* );
};

#endif  //INSTALLATION_PROCESS_H
