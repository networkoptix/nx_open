#ifndef BACKUP_STATUS_REPLY_H
#define BACKUP_STATUS_REPLY_H

#include <QtCore/QMetaType>

#include <utils/common/model_functions_fwd.h>

struct QnBackupStatusData
{
    QnBackupStatusData(): state(Qn::BackupState_None), progress(0.0), backupTimeMs(0) {}
    QnBackupStatusData(Qn::BackupState state, qreal progress): state(state), progress(progress) {}

    Qn::BackupState state;
    qreal progress;
	qint64 backupTimeMs; // current datetime of backuped data
};
#define QnBackupStatusData_Fields (state)(progress)(backupTimeMs)

QN_FUSION_DECLARE_FUNCTIONS(QnBackupStatusData, (json)(metatype))



#endif // BACKUP_STATUS_REPLY_H
