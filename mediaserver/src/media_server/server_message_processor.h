#ifndef QN_SERVER_MESSAGE_PROCESSOR_H
#define QN_SERVER_MESSAGE_PROCESSOR_H

#include <QtCore/QWaitCondition>
#include <api/common_message_processor.h>

#include <core/resource/resource.h>

class QnServerMessageProcessor : public QnCommonMessageProcessor
{
    Q_OBJECT

    typedef QnCommonMessageProcessor base_type;
public:
    QnServerMessageProcessor();

    void run();

    void resume();

protected:
    virtual void loadRuntimeInfo(const QnMessage &message) override;
    virtual void handleConnectionOpened(const QnMessage &message) override;
    virtual void handleConnectionClosed(const QString &errorString) override;
    virtual void handleMessage(const QnMessage &message) override;

private:
    void pause();
    void updateResource(const QnResourcePtr& resource);

private slots:
    void at_aboutToBeStarted();
    void at_serverSaved(int status, const QnResourceList &, int);

signals:
    void aboutToBeStarted();

private:
    QScopedPointer<QThread> m_thread;

    bool m_paused;
    QMutex m_mutex;
    QWaitCondition m_cond;
};

#endif // QN_SERVER_MESSAGE_PROCESSOR_H
