#ifndef __RECORDING_STATUS_HELPER_H__
#define __RECORDING_STATUS_HELPER_H__

#include <QIcon>
#include <core/resource/resource_fwd.h>

class QnWorkbenchContext;

class QnRecordingStatusHelper
{
public:
    static int currentRecordingMode(QnWorkbenchContext *context, QnVirtualCameraResourcePtr camera);
    static QString tooltip(int recordingMode);
    static QString shortTooltip(int recordingMode);
    static QIcon icon(int recordingMode);
};

#endif // __RECORDING_STATUS_HELPER_H__
