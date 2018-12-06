#pragma once

#ifndef Q_MOC_RUN
#include <boost/optional.hpp>
#endif

#include <nx/fusion/model_functions_fwd.h>

// TODO: #GDM make it template class if it will be required somewhere else

class QnOptionalBool {
public:
    QnOptionalBool();
    QnOptionalBool(bool value);

    bool value() const;
    void setValue(bool value);

    bool isDefined() const;
    void undefine();

    bool operator ==(const QnOptionalBool &other) const;
private:
    /* Should be used with care. boost::tribool is much better but is not currently included in our distribution. */
    boost::optional<bool> m_value;
};

QN_FUSION_DECLARE_FUNCTIONS(QnOptionalBool, (metatype)(lexical));