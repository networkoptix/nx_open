#ifndef ABSTRACT_KVPAIR_WATCHER_H
#define ABSTRACT_KVPAIR_WATCHER_H

#include <QtCore/QObject>

class QnAbstractKvPairWatcher: public QObject {
    Q_OBJECT
public:
    QnAbstractKvPairWatcher(QObject *parent = 0): QObject(parent) {}
    virtual ~QnAbstractKvPairWatcher() {}

    virtual QString key() const = 0;

    virtual void updateValue(int resourceId, const QString &value) = 0;
    virtual void removeValue(int resourceId) = 0;

protected:
    void submitValue(int resourceId, const QString &value) {
        emit valueModified(resourceId, value);
    }

signals:
    void valueModified(int resourceId, const QString &value);
};


#endif // ABSTRACT_KVPAIR_WATCHER_H
