#pragma once

#include <core/resource/resource_fwd.h>
#include <common/common_globals.h>
#include <common/common_module_aware.h>

class QnResourceDisplayInfo: public QnCommonModuleAware
{
public:
    QnResourceDisplayInfo(QnCommonModule* commonModule, const QnResourcePtr& resource);

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
    mutable Qn::ResourceInfoLevel m_detailLevel;

    mutable QString m_name;
    mutable QString m_host;
    mutable int m_port;
    mutable QString m_extraInfo;
};
