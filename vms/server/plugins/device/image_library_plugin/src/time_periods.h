// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <list>
#include <utility>

#include <camera/camera_plugin.h>
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
    virtual int addRef() const override;
    //!Implementation of nxpl::PluginInterface::releaseRef
    virtual int releaseRef() const override;

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
