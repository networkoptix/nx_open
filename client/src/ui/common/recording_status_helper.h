#ifndef __RECORDING_STATUS_HELPER_H__
#define __RECORDING_STATUS_HELPER_H__

#include <QtCore/QObject>
#include <QtGui/QIcon>
#include <core/resource/resource_fwd.h>

class QnWorkbenchContext;

class QnRecordingStatusHelper: public QObject {
    Q_OBJECT
public:
    static int currentRecordingMode(QnWorkbenchContext *context, QnVirtualCameraResourcePtr camera);
    static QString tooltip(int recordingMode);
    static QString shortTooltip(int recordingMode);
    static QIcon icon(int recordingMode);
};

#endif // __RECORDING_STATUS_HELPER_H__
