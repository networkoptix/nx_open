#pragma once

#include <nx/utils/thread/stoppable.h>
#include <nx/utils/thread/barrier_handler.h>

namespace nx::cloud::db {

class CloudDbService;

class CloudDbServicePublic:
    public QnStoppable
{
public:
    CloudDbServicePublic(int argc, char **argv);
    virtual ~CloudDbServicePublic();

    virtual void pleaseStop() override;

    void setOnStartedEventHandler(
        nx::utils::MoveOnlyFunc<void(bool /*result*/)> handler);

    int exec();

    const CloudDbService* impl() const;
    CloudDbService* impl();

private:
    CloudDbService* m_impl;
};

} // namespace nx::cloud::db
