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
        QByteArray /*serializedNotification*/,
        std::function<void(bool)> completionHandler) override
    {
        //sendAsyncMocked(std::move(serializedNotification));
        sendAsyncMocked(QByteArray());
        if (completionHandler)
            completionHandler(true);
    }
};

#endif  //EMAIL_MANAGER_MOCKED_H
