#ifndef QN_SCREEN_RECORDING_WRAPPER_H
#define QN_SCREEN_RECORDING_WRAPPER_H

#include <QObject>

class QnScreenRecordingWrapper: public QObject {
    Q_OBJECT;
public:
    QnScreenRecordingWrapper(QObject *parent = NULL);

    virtual ~QnScreenRecordingWrapper();

};

#endif // QN_SCREEN_RECORDING_WRAPPER_H
