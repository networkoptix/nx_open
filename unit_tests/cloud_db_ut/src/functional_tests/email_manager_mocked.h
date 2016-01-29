/**********************************************************
* Dec 18, 2015
* a.kolesnikov
***********************************************************/

#ifndef EMAIL_MANAGER_MOCKED_H
#define EMAIL_MANAGER_MOCKED_H

#include <gmock/gmock.h>

#include <libcloud_db/src/managers/email_manager.h>


class EmailManagerMocked
:
    public nx::cdb::AbstractEmailManager
{
public:
    MOCK_METHOD1(
        sendAsyncMocked,
        void(QByteArray serializedNotification));

    virtual void sendAsync(
        QByteArray serializedNotification,
        std::function<void(bool)> completionHandler) override
    {
        //sendAsyncMocked(std::move(serializedNotification));
        sendAsyncMocked(QByteArray());
        if (completionHandler)
            completionHandler(true);
    }
};

class EmailManagerStub
:
    public nx::cdb::AbstractEmailManager
{
public:
    EmailManagerStub(nx::cdb::AbstractEmailManager* const target)
    :
        m_target(target)
    {
    }

    virtual void sendAsync(
        QByteArray serializedNotification,
        std::function<void(bool)> completionHandler) override
    {
        if (!m_target)
            return;
        m_target->sendAsync(
            std::move(serializedNotification),
            std::move(completionHandler));
    }

private:
    nx::cdb::AbstractEmailManager* const m_target;
};

#endif  //EMAIL_MANAGER_MOCKED_H
