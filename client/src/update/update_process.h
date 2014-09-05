#ifndef QN_UPDATE_PROCESS_H
#define QN_UPDATE_PROCESS_H

#include <core/resource/resource_fwd.h>

#include <update/updates_common.h>

#include <utils/common/software_version.h>
#include <utils/common/system_information.h>

struct QnPeerUpdateInformation {
    enum State {
        UpdateUnknown,
        UpdateNotFound,
        UpdateFound,
        PendingDownloading,
        UpdateDownloading,
        PendingUpload,
        UpdateUploading,
        PendingInstallation,
        UpdateInstalling,
        UpdateFinished,
        UpdateFailed,
        UpdateCanceled
    };

    QnMediaServerResourcePtr server;
    State state;
    QnSoftwareVersion sourceVersion;
    QnUpdateFileInformationPtr updateInformation;

    int progress;

    QnPeerUpdateInformation(const QnMediaServerResourcePtr &server = QnMediaServerResourcePtr());
};

struct QnUpdateProcess {
    QUuid id;
    QHash<QnSystemInformation, QnUpdateFileInformationPtr> updateFiles;
    QString localTemporaryDir;
    QnSoftwareVersion targetVersion;
    bool clientRequiresInstaller;
    QnUpdateFileInformationPtr clientUpdateFile;
    QHash<QUuid, QnPeerUpdateInformation> updateInformationById;
    QMultiHash<QnSystemInformation, QUuid> idBySystemInformation;
};


#endif //QN_UPDATE_PROCESS_H