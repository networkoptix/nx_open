// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>

#include <core/ptz/ptz_constants.h>
#include <core/ptz/ptz_fwd.h>

class NX_VMS_COMMON_API QnPtzAuxiliaryTrait
{
public:
    QnPtzAuxiliaryTrait(): m_standardTrait(Ptz::NoPtzTraits) {}
    QnPtzAuxiliaryTrait(const QString &name);
    QnPtzAuxiliaryTrait(Ptz::Trait standardTrait);

    const QString &name() const {
        return m_name;
    }

    Ptz::Trait standardTrait() const {
        return m_standardTrait;
    }

    friend bool operator==(const QnPtzAuxiliaryTrait &l, const QnPtzAuxiliaryTrait &r) {
        return l.m_name == r.m_name;
    }

private:
    Ptz::Trait m_standardTrait;
    QString m_name;
};
