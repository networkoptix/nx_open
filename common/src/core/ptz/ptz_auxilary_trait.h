#ifndef QN_PTZ_AUXILARY_TRAIT_H
#define QN_PTZ_AUXILARY_TRAIT_H

#include <QtCore/QString>

#include <boost/operators.hpp>

#include "ptz_fwd.h"


class QnPtzAuxilaryTrait: public boost::equality_comparable1<QnPtzAuxilaryTrait> {
public:
    QnPtzAuxilaryTrait(): m_standardTrait(Qn::NoPtzTraits) {}
    QnPtzAuxilaryTrait(const QString &name);
    QnPtzAuxilaryTrait(Qn::PtzTrait standardTrait);

    const QString &name() const {
        return m_name;
    }

    Qn::PtzTrait standardTrait() const {
        return m_standardTrait;
    }

    friend bool operator==(const QnPtzAuxilaryTrait &l, const QnPtzAuxilaryTrait &r) {
        return l.m_name == r.m_name;
    }

private:
    Qn::PtzTrait m_standardTrait;
    QString m_name;
};

Q_DECLARE_METATYPE(QnPtzAuxilaryTrait)
Q_DECLARE_METATYPE(QnPtzAuxilaryTraitList);


#endif // QN_PTZ_AUXILARY_TRAIT_H

