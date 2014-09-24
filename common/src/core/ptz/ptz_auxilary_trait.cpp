#include "ptz_auxilary_trait.h"

#include <utils/common/model_functions.h>

QnPtzAuxilaryTrait::QnPtzAuxilaryTrait(const QString &name):
    m_standardTrait(QnLexical::deserialized<Qn::PtzTrait>(name, Qn::NoPtzTraits)),
    m_name(name)
{}

QnPtzAuxilaryTrait::QnPtzAuxilaryTrait(Qn::PtzTrait standardTrait):
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
