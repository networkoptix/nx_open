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
#include <nx/utils/thread/stoppable.h>
#include <nx/utils/thread/joinable.h>
#include <nx/network/deprecated/asynchttpclient.h>
#include <nx/utils/stree/node.h>
#include <nx/utils/stree/resourcenameset.h>

#include "rdir_syncher.h"
#include "rns_product_parameters.h"


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
        const QnSoftwareVersion& version,
        const QString& module,
        const QString& installationDirectory,
        bool autoStartNeeded );
    virtual ~InstallationProcess();

    //!Implementation of QnStoppable::pleaseStop()
    virtual void pleaseStop() override;
    //!Implementation of QnJoinable::join()
    virtual void join() override;

    bool start( const QString& mirrorListUrl );

    applauncher::api::InstallationStatus::Value getStatus() const;
    //!Returns installation progress (percents)
    float getProgress() const;
    bool autoStartNeeded() const;

    QString errorText() const;

    QnSoftwareVersion getVersion() const;

    void cancel();

signals:
    void installationDone( InstallationProcess* installationProcess );

private:
    enum class State
    {
        init,
        downloadMirrorList,
        installing,
        finished
    };

    const ProductResourcesNameset m_rns;
    const QString m_productName;
    const QString m_customization;
    const QnSoftwareVersion m_version;
    const QString m_module;
    const QString m_installationDirectory;
    State m_state;
    nx_http::AsyncHttpClientPtr m_httpClient;
    applauncher::api::InstallationStatus::Value m_status;
    std::shared_ptr<RDirSyncher> m_syncher;
    QString m_errorText;
    mutable std::mutex m_mutex;
    std::unique_ptr<nx::utils::stree::AbstractNode> m_currentTree;
    int64_t m_totalBytesDownloaded;
    std::map<QString, int64_t> m_unfinishedFilesBytesDownloaded;
    int64_t m_totalBytesToDownload;
    bool m_autoStartNeeded;
    QMap<QString, qint64> m_fileSizeByEntry;

    //!Implementation of RDirSyncher::EventReceiver::overrallDownloadSizeKnown
    virtual void overrallDownloadSizeKnown(
        const std::shared_ptr<RDirSyncher>& syncher,
        int64_t totalBytesToDownload );
    //!Implementation of RDirSyncher::EventReceiver::fileProgress
    virtual void fileProgress(
        const std::shared_ptr<RDirSyncher>& syncher,
        const QString& filePath,
        int64_t remoteFileSize,
        int64_t bytesDownloaded ) override;
    //!Implementation of RDirSyncher::EventReceiver::fileDone
    virtual void fileDone(
        const std::shared_ptr<RDirSyncher>& syncher,
        const QString& filePath ) override;
    //!Implementation of RDirSyncher::EventReceiver::finished
    virtual void finished(
        const std::shared_ptr<RDirSyncher>& syncher,
        bool result ) override;
    //!Implementation of RDirSyncher::EventReceiver::failed
    virtual void failed(
        const std::shared_ptr<RDirSyncher>& syncher,
        const QString& failedFilePath,
        const QString& errorText ) override;

    bool writeInstallationSummary();

private slots:
    void onHttpDone( nx_http::AsyncHttpClientPtr );
};

#endif  //INSTALLATION_PROCESS_H
