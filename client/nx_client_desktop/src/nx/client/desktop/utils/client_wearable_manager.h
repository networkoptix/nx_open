#pragma once

#include <QtCore/QObject>

class QnClientWearableManager: public QObject
{
    Q_OBJECT
public:
    QnClientWearableManager(QObject* parent = nullptr);
    ~QnClientWearableManager();

    void tehProgress(qreal aaaaaaa) {
        emit progress(aaaaaaa);
    }

signals:
    void progress(qreal progress);
};

