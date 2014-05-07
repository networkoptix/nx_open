/**********************************************************
* 10 sep 2013
* akolesnikov@networkoptix.com
***********************************************************/

#ifndef TIME_PERIODS_H
#define TIME_PERIODS_H

#include <list>
#include <utility>

#include <plugins/camera_plugin.h>

#include <plugins/plugin_tools.h>


class TimePeriods
:
    public nxcip::TimePeriods
{
public:
    typedef std::list<std::pair<nxcip::UsecUTCTimestamp, nxcip::UsecUTCTimestamp> > container_type;
    container_type timePeriods;

    TimePeriods();

    //!Implementation of nxpl::PluginInterface::queryInterface
    virtual void* queryInterface( const nxpl::NX_GUID& interfaceID ) override;
    //!Implementation of nxpl::PluginInterface::addRef
    virtual unsigned int addRef() override;
    //!Implementation of nxpl::PluginInterface::releaseRef
    virtual unsigned int releaseRef() override;

    //!Implementation of nxcip::TimePeriods::get
    virtual bool get( nxcip::UsecUTCTimestamp* start, nxcip::UsecUTCTimestamp* end ) const override;
    //!Implementation of nxcip::TimePeriods::goToBeginning
    virtual void goToBeginning() override;
    //!Implementation of nxcip::TimePeriods::next
    virtual bool next() override;
    //!Implementation of nxcip::TimePeriods::atEnd
    virtual bool atEnd() const override;

private:
    nxpt::CommonRefManager m_refManager;
    container_type::const_iterator m_pos;
};

#endif //TIME_PERIODS_H
