#ifndef __RECORDING_MANAGER_H__
#define __RECORDING_MANAGER_H__

#include <QObject>
#include "core/resource/resource.h"

class QnRecordingManager: public QObject
{
    Q_OBJECT
public:
    QnRecordingManager();
    virtual ~QnRecordingManager();

    void start();
public slots:
    void onNewResource(QnResourcePtr res);
    void onRemoveResource(QnResourcePtr res);
    void recordingFailed(QString errMessage);
private:
    //QnStreamRecorder* m_recorder;
};

#endif // __RECORDING_MANAGER_H__
