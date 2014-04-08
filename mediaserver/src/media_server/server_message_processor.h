#ifndef QN_SERVER_MESSAGE_PROCESSOR_H
#define QN_SERVER_MESSAGE_PROCESSOR_H

#include <api/common_message_processor.h>

#include <core/resource/resource.h>

class QnServerMessageProcessor : public QnCommonMessageProcessor
{
    Q_OBJECT

    typedef QnCommonMessageProcessor base_type;
public:
    QnServerMessageProcessor();

    void run();

protected:
    virtual void loadRuntimeInfo(const QnMessage &message) override;
    virtual void handleConnectionOpened(const QnMessage &message) override;
    virtual void handleConnectionClosed(const QString &errorString) override;
    virtual void handleMessage(const QnMessage &message) override;

private:
    void updateResource(const QnResourcePtr& resource);

private slots:
    void at_aboutToBeStarted();
    void at_serverSaved(int status, const QnResourceList &, int);

signals:
    void aboutToBeStarted();

private:
    QScopedPointer<QThread> m_thread;
};

#endif // QN_SERVER_MESSAGE_PROCESSOR_H
