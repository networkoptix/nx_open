#pragma once

class QnLayoutStreamSupport
{
public:
    virtual ~QnLayoutStreamSupport() = default;

    virtual void lockFile() = 0;
    virtual void unlockFile() = 0;

    virtual void storeStateAndClose() = 0 ;
    virtual void restoreState() = 0;
};
