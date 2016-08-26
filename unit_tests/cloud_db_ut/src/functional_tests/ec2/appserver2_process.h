/**********************************************************
* Aug 25, 2016
* a.kolesnikov
***********************************************************/

#pragma once

#include <QtCore/QCoreApplication>

#include <nx/utils/std/future.h>

#include <utils/common/stoppable.h>


//namespace nx {
namespace ec2 {

class AbstractECConnection;

class Appserver2Process
    :
    public QnStoppable
{
public:
    Appserver2Process(int argc, char **argv);
    virtual ~Appserver2Process();

    virtual void pleaseStop() override;

    void setOnStartedEventHandler(
        nx::utils::MoveOnlyFunc<void(bool /*result*/)> handler);

    int exec();

    ec2::AbstractECConnection* ecConnection();

private:
    int m_argc;
    char** m_argv;
    bool m_terminated;
    nx::utils::promise<void> m_processTerminationEvent;
    nx::utils::MoveOnlyFunc<void(bool /*result*/)> m_onStartedEventHandler;
    AbstractECConnection* m_ecConnection;
    QCoreApplication m_application;
};


class Appserver2ProcessPublic
    :
    public QnStoppable
{
public:
    Appserver2ProcessPublic(int argc, char **argv);
    virtual ~Appserver2ProcessPublic();

    virtual void pleaseStop() override;

    void setOnStartedEventHandler(
        nx::utils::MoveOnlyFunc<void(bool /*result*/)> handler);

    int exec();

    const Appserver2Process* impl() const;
    ec2::AbstractECConnection* ecConnection();

private:
    Appserver2Process* m_impl;
};

}   // namespace ec2
//}   // namespace nx
