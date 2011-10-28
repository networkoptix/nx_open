#ifndef __RECORDING_MANAGER_H__
#define __RECORDING_MANAGER_H__

#include <QObject>
#include "core/resource/resource.h"

class QnRecordingManager: public QObject
{
    Q_OBJECT
public:
    QnRecordingManager();
public slots:
    void onNewResource(QnResourcePtr res);
    void onRemoveResource(QnResourcePtr res);
};

#endif // __RECORDING_MANAGER_H__
