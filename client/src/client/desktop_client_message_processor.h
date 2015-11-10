#ifndef QNDESKTOPCLIENTMESSAGEPROCESSOR_H
#define QNDESKTOPCLIENTMESSAGEPROCESSOR_H

#include <client/client_message_processor.h>

class QnDesktopClientMessageProcessor : public QnClientMessageProcessor
{
    Q_OBJECT

    typedef QnClientMessageProcessor base_type;

public:
    QnDesktopClientMessageProcessor();

    QnIncompatibleServerWatcher *incompatibleServerWatcher() const;

protected:
    virtual void connectToConnection(const ec2::AbstractECConnectionPtr &connection) override;
    virtual void disconnectFromConnection(const ec2::AbstractECConnectionPtr &connection) override;

private slots:
    void at_gotInitialModules(const QList<QnModuleInformationWithAddresses> &modules);

private:
    QnIncompatibleServerWatcher *m_incompatibleServerWatcher;
};

#define qnDesktopClientMessageProcessor static_cast<QnDesktopClientMessageProcessor*>(QnDesktopClientMessageProcessor::instance())

#endif // QNDESKTOPCLIENTMESSAGEPROCESSOR_H
