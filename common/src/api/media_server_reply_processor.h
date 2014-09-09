#ifndef QN_MEDIA_SERVER_REPLY_PROCESSOR_H
#define QN_MEDIA_SERVER_REPLY_PROCESSOR_H

#include "abstract_reply_processor.h"

#include <core/ptz/ptz_fwd.h>
#include <core/resource/camera_bookmark_fwd.h>
#include <business/business_fwd.h>

#include <api/api_fwd.h>
#include <api/model/camera_diagnostics_reply.h>
#include <api/model/storage_space_reply.h>
#include <api/model/storage_status_reply.h>
#include <api/model/statistics_reply.h>
#include <api/model/time_reply.h>
#include "api/model/test_email_settings_reply.h"
#include <api/model/rebuild_archive_reply.h>
#include <api/model/manual_camera_seach_reply.h>
#include <api/model/camera_list_reply.h>

#include "media_server_connection.h"

class QnTimePeriodList;

class QnMediaServerReplyProcessor: public QnAbstractReplyProcessor {
    Q_OBJECT

public:
    QnMediaServerReplyProcessor(int object): QnAbstractReplyProcessor(object) {}

    virtual void processReply(const QnHTTPRawResponse &response, int handle) override;

signals:
    void finished(int status, const QnRebuildArchiveReply &reply, int handle);
    void finished(int status, const QnCameraListReply &reply, int handle);
    void finished(int status, const QnStorageStatusReply &reply, int handle);
    void finished(int status, const QnStorageSpaceReply &reply, int handle);
    void finished(int status, const QnTimePeriodList &reply, int handle);
    void finished(int status, const QnStatisticsReply &reply, int handle);
    void finished(int status, const QVector3D &reply, int handle);
    void finished(int status, const QnStringVariantPairList &reply, int handle);
    void finished(int status, const QnStringBoolPairList &reply, int handle);
    void finished(int status, const QnTimeReply &reply, int handle);
    void finished(int status, const QnTestEmailSettingsReply &reply, int handle);
    void finished(int status, const QnCameraDiagnosticsReply &reply, int handle);
    void finished(int status, const QnManualCameraSearchReply &reply, int handle);
    void finished(int status, const QnBusinessActionDataListPtr &reply, int handle);
    void finished(int status, const QImage &reply, int handle);
    void finished(int status, const QString &reply, int handle);
    void finished(int status, const QnPtzPresetList &reply, int handle);
    void finished(int status, const QnPtzTourList &reply, int handle);
    void finished(int status, const QnPtzObject &reply, int handle);
    void finished(int status, const QnPtzAuxilaryTraitList &reply, int handle);
    void finished(int status, const QnPtzData &reply, int handle);
    void finished(int status, const QnCameraBookmark &reply, int handle);
    void finished(int status, const QnCameraBookmarkList &reply, int handle);

private:
    friend class QnAbstractReplyProcessor;
};


#endif // QN_MEDIA_SERVER_REPLY_PROCESSOR_H
