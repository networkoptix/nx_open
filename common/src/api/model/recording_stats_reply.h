#ifndef NX_RECORDING_STATS_DATA_H
#define NX_RECORDING_STATS_DATA_H

#include <utils/common/uuid.h>
#include <utils/common/model_functions_fwd.h>
#include <vector>

//!mediaserver's response to \a recording stats request
struct QnRecordingStatsData
{
public:
    QnRecordingStatsData(): recordedBytes(0), recordedSecs(0), averageBitrate(0) {}

    qint64 recordedBytes;  // recorded archive in bytes
    qint64 recordedSecs;     // recorded archive in seconds
    qint64 averageBitrate; // average bitrate in bytes/sec
    
    void operator +=(const QnRecordingStatsData& right) {
        recordedBytes += right.recordedBytes;
        recordedSecs += right.recordedSecs;
    }

};
#define QnRecordingStatsData_Fields (recordedBytes)(recordedSecs)(averageBitrate)

struct QnCamRecordingStatsData: public QnRecordingStatsData
{
    QnCamRecordingStatsData(): QnRecordingStatsData() {}
    QnCamRecordingStatsData(const QnRecordingStatsData& value): QnRecordingStatsData(value) {}

    QnUuid id;
};
#define QnCamRecordingStatsData_Fields (id) QnRecordingStatsData_Fields

QN_FUSION_DECLARE_FUNCTIONS(QnRecordingStatsData, (json))
QN_FUSION_DECLARE_FUNCTIONS(QnCamRecordingStatsData, (json))

typedef QVector<QnCamRecordingStatsData> QnRecordingStatsReply;
Q_DECLARE_METATYPE(QnCamRecordingStatsData)
Q_DECLARE_METATYPE(QnRecordingStatsReply)


#endif  //NX_RECORDING_STATS_DATA_H
