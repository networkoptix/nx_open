#include "ptz_auxilary_trait.h"

#include <nx/fusion/model_functions.h>

QnPtzAuxilaryTrait::QnPtzAuxilaryTrait(const QString &name):
    m_standardTrait(QnLexical::deserialized<Ptz::Trait>(name, Ptz::NoPtzTraits)),
    m_name(name)
{}

QnPtzAuxilaryTrait::QnPtzAuxilaryTrait(Ptz::Trait standardTrait):
    m_standardTrait(standardTrait),
    m_name(QnLexical::serialized(standardTrait))
{}

void serialize(const QnPtzAuxilaryTrait &value, QString *target) {
    *target = value.name();
}

bool deserialize(const QString &value, QnPtzAuxilaryTrait *target) {
    *target = QnPtzAuxilaryTrait(value);
    return true;
}

QN_FUSION_DEFINE_FUNCTIONS(QnPtzAuxilaryTrait, (json_lexical))
