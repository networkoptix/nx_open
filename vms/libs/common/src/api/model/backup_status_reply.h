#ifndef BACKUP_STATUS_REPLY_H
#define BACKUP_STATUS_REPLY_H

#include <QtCore/QMetaType>

#include <nx/fusion/model_functions_fwd.h>

struct QnBackupStatusData
{
    QnBackupStatusData(): state(Qn::BackupState_None), progress(0.0), backupTimeMs(0) {}
    QnBackupStatusData(Qn::BackupState state, qreal progress): state(state), progress(progress) {}

    Qn::BackupState state;
    qreal progress;

    // TODO: #rvasilenko why start time? Should we automatically add 2 minutes on the client side?
    qint64 backupTimeMs; /**< Synchronized utc start time of the last chunk, that was backed up. */     
};
#define QnBackupStatusData_Fields (state)(progress)(backupTimeMs)

QN_FUSION_DECLARE_FUNCTIONS(QnBackupStatusData, (json)(metatype)(eq))



#endif // BACKUP_STATUS_REPLY_H
