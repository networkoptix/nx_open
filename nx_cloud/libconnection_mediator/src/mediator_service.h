/**********************************************************
* 27 aug 2013
* a.kolesnikov
***********************************************************/

#ifndef HOLE_PUNCHER_SERVICE_H
#define HOLE_PUNCHER_SERVICE_H

#include <memory>

#include <QtCore/QSettings>

#include <qtsinglecoreapplication.h>
#include <qtservice.h>

#include <nx/utils/move_only_func.h>
#include <utils/common/stoppable.h>


namespace nx {
namespace hpm {

namespace conf {
    class Settings;
}

class MediatorProcess
:
    public QtService<QtSingleCoreApplication>,
    public QnStoppable
{
public:
    MediatorProcess(int argc, char **argv);

    void setOnStartedEventHandler(
        nx::utils::MoveOnlyFunc<void(bool /*result*/)> handler);

    //!Implementation of QnStoppable::pleaseStop
    virtual void pleaseStop() override;

protected:
    virtual int executeApplication() override;
    virtual void start() override;
    virtual void stop() override;

private:
    std::unique_ptr<QSettings> m_settings;
    int m_argc;
    char** m_argv;
    nx::utils::MoveOnlyFunc<void(bool /*result*/)> m_startedEventHandler;

    QString getDataDirectory();
    int printHelp();
    void initializeLogging(const conf::Settings& settings);
};

} // namespace hpm
} // namespace nx

#endif  //HOLE_PUNCHER_SERVICE_H
