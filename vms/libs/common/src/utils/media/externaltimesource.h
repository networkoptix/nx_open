#pragma once

#include <QtCore/QtGlobal>

class QnlTimeSource
{
public:
    /*
    * @return current time. May be different from displayed time. After seek for example, while no any frames are really displayed
    */
    virtual qint64 getCurrentTime() const = 0;

    /*
    * @return Return last displayed time
    */
    virtual qint64 getDisplayedTime() const = 0;

    /*
    * @return Return time of the next frame
    */
    virtual qint64 getNextTime() const = 0;

    /*
    * @return time of the external time source. Syncplay time of camDisplay for example
    */
    virtual qint64 getExternalTime() const = 0;

    /*
    * @return expected time. For example, time based on local timer
    */
    virtual qint64 expectedTime() const { return getCurrentTime(); }

    virtual void onBufferingStarted(QnlTimeSource* /*src*/, qint64 /*firstTime*/) {}
    virtual void onBufferingFinished(QnlTimeSource* /*src*/) {}
    virtual void onEofReached(QnlTimeSource* /*src*/, bool /*value*/) {}
    virtual bool isEnabled() const { return true; }

    virtual void reinitTime(qint64 /*newTime*/) {}

    virtual bool isBuffering() const { return false; }
};
