// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ptz_auxiliary_trait.h"

#include <nx/fusion/model_functions.h>
#include <nx/reflect/string_conversion.h>

QnPtzAuxiliaryTrait::QnPtzAuxiliaryTrait(const QString &name):
    m_standardTrait(nx::reflect::fromString<Ptz::Trait>(name.toStdString(), Ptz::NoPtzTraits)),
    m_name(name)
{}

QnPtzAuxiliaryTrait::QnPtzAuxiliaryTrait(Ptz::Trait standardTrait):
    m_standardTrait(standardTrait),
    m_name(QString::fromStdString(nx::reflect::toString(standardTrait)))
{}

void serialize(const QnPtzAuxiliaryTrait &value, QString *target) {
    *target = value.name();
}

bool deserialize(const QString &value, QnPtzAuxiliaryTrait *target) {
    *target = QnPtzAuxiliaryTrait(value);
    return true;
}

QN_FUSION_DEFINE_FUNCTIONS(QnPtzAuxiliaryTrait, (json_lexical))
