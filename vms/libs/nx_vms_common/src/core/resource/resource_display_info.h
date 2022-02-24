// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_fwd.h>
#include <common/common_globals.h>

class NX_VMS_COMMON_API QnResourceDisplayInfo
{
public:
    QnResourceDisplayInfo(const QnResourcePtr& resource);

    QString name() const;
    QString host() const;
    int port() const;
    QString extraInfo() const;

    QString toString(Qn::ResourceInfoLevel detailLevel) const;

private:
    void ensureConstructed(Qn::ResourceInfoLevel detailLevel) const;

private:
    QnResourcePtr m_resource;

    /** How deep are have constructed the info. */
    mutable Qn::ResourceInfoLevel m_detailLevel = Qn::RI_Invalid;

    mutable QString m_name;
    mutable QString m_host;
    mutable int m_port = 0;
    mutable QString m_extraInfo;
};
