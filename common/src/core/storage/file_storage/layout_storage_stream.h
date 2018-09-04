#pragma once

#include <QtCore/QtGlobal>
#include <QtCore/QIODevice>

class QnLayoutStream: public QIODevice
{
public:
    // // Includes any stream metadata and padding.
    // virtual qint64 grossSize() const = 0;

    virtual void lockFile() = 0;
    virtual void unlockFile() = 0;

    virtual void storeStateAndClose() = 0 ;
    virtual void restoreState() = 0;
};
