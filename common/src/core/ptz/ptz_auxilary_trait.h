#ifndef QN_PTZ_AUXILARY_TRAIT_H
#define QN_PTZ_AUXILARY_TRAIT_H

#include <QtCore/QString>

#ifndef Q_MOC_RUN
#include <boost/operators.hpp>
#endif

#include <core/ptz/ptz_fwd.h>
#include <core/ptz/ptz_constants.h>

class QnPtzAuxilaryTrait: public boost::equality_comparable1<QnPtzAuxilaryTrait> {
public:
    QnPtzAuxilaryTrait(): m_standardTrait(Ptz::NoPtzTraits) {}
    QnPtzAuxilaryTrait(const QString &name);
    QnPtzAuxilaryTrait(Ptz::Trait standardTrait);

    const QString &name() const {
        return m_name;
    }

    Ptz::Trait standardTrait() const {
        return m_standardTrait;
    }

    friend bool operator==(const QnPtzAuxilaryTrait &l, const QnPtzAuxilaryTrait &r) {
        return l.m_name == r.m_name;
    }

private:
    Ptz::Trait m_standardTrait;
    QString m_name;
};

Q_DECLARE_METATYPE(QnPtzAuxilaryTrait)
Q_DECLARE_METATYPE(QnPtzAuxilaryTraitList)


#endif // QN_PTZ_AUXILARY_TRAIT_H

