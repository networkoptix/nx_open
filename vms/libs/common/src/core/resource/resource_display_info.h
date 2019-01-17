#pragma once

#include <core/resource/resource_fwd.h>
#include <common/common_globals.h>

class QnResourceDisplayInfo
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
