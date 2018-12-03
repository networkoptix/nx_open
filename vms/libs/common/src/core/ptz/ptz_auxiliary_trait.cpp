#include "ptz_auxiliary_trait.h"

#include <nx/fusion/model_functions.h>

QnPtzAuxiliaryTrait::QnPtzAuxiliaryTrait(const QString &name):
    m_standardTrait(QnLexical::deserialized<Ptz::Trait>(name, Ptz::NoPtzTraits)),
    m_name(name)
{}

QnPtzAuxiliaryTrait::QnPtzAuxiliaryTrait(Ptz::Trait standardTrait):
    m_standardTrait(standardTrait),
    m_name(QnLexical::serialized(standardTrait))
{}

void serialize(const QnPtzAuxiliaryTrait &value, QString *target) {
    *target = value.name();
}

bool deserialize(const QString &value, QnPtzAuxiliaryTrait *target) {
    *target = QnPtzAuxiliaryTrait(value);
    return true;
}

QN_FUSION_DEFINE_FUNCTIONS(QnPtzAuxiliaryTrait, (json_lexical))
