#include "ptz_auxilary_trait.h"

#include <utils/common/lexical.h>
#include <utils/common/json.h>

QnPtzAuxilaryTrait::QnPtzAuxilaryTrait(const QString &name):
    m_name(name),
    m_standardTrait(QnLexical::deserialized<Qn::PtzTrait>(name, Qn::NoPtzTraits))
{}

QnPtzAuxilaryTrait::QnPtzAuxilaryTrait(Qn::PtzTrait standardTrait):
    m_name(QnLexical::serialized(standardTrait)),
    m_standardTrait(standardTrait)
{}

void serialize(const QnPtzAuxilaryTrait &value, QString *target) {
    *target = value.name();
}

bool deserialize(const QString &value, QnPtzAuxilaryTrait *target) {
    *target = QnPtzAuxilaryTrait(value);
    return true;
}

QN_DEFINE_LEXICAL_JSON_SERIALIZATION_FUNCTIONS(QnPtzAuxilaryTrait)
