#ifndef __RECORDING_STATUS_HELPER_H__
#define __RECORDING_STATUS_HELPER_H__

#include <QtCore/QObject>
#include <QtGui/QIcon>
#include <core/resource/resource_fwd.h>

class QnWorkbenchContext;

// TODO: #Elric #TR rename, *helper suffix usually means "I can't think of a meaningful name, so I'll just call it helper".

class QnRecordingStatusHelper: public QObject {
    Q_OBJECT
public:
    static int currentRecordingMode(const QnVirtualCameraResourcePtr &camera);
    static QString tooltip(int recordingMode);
    static QString shortTooltip(int recordingMode);
    static QIcon icon(int recordingMode);
};

#endif // __RECORDING_STATUS_HELPER_H__
