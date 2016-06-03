#pragma once

#include <core/resource/resource_fwd.h>

class QnResourceDisplayInfo
{
public:
    QnResourceDisplayInfo(const QnResourcePtr& resource);

    QString name() const;
    QString url() const;
    QString extraInfo() const;

    QString toString(Qn::ResourceInfoLevel detailLevel) const;

private:
    void ensureConstructed(Qn::ResourceInfoLevel detailLevel) const;

private:
    QnResourcePtr m_resource;

    /** How deep are have constructed the info. */
    mutable Qn::ResourceInfoLevel m_detailLevel;

    mutable QString m_name;
    mutable QString m_url;
    mutable QString m_extraInfo;

};
