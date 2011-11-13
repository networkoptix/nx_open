#ifndef QN_CONSTRAINED_RESIZABLE
#define QN_CONSTRAINED_RESIZABLE

#include <QSizeF>

class ConstrainedResizable {
public:
    /**
     * This function is to be called just before the derived class's geometry
     * is to be changed.
     *
     * Given a size constraint, it returns a preferred size that satisfies it. 
     * The result of this function must not exceed \a constraint, but it need 
     * not be equal to it.
     *
     * \param constraint                Constraint for the result. Must be a valid size.
     * \returns                         Preferred size for the given constraint.
     */
    virtual QSizeF constrainedSize(const QSizeF constraint) const = 0;
};


#endif // QN_CONSTRAINED_RESIZABLE
