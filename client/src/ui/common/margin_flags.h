#ifndef QN_MARGIN_FLAGS_H
#define QN_MARGIN_FLAGS_H

namespace Qn {
    enum MarginFlag {
        /** Viewport margins affect how viewport size is bounded. */
        MarginsAffectSize = 0x1,        

        /** Viewport margins affect how viewport position is bounded. */
        MarginsAffectPosition = 0x2,
    };

    Q_DECLARE_FLAGS(MarginFlags, MarginFlag);
} // namespace Qn

Q_DECLARE_OPERATORS_FOR_FLAGS(Qn::MarginFlags);

#endif // QN_MARGIN_FLAGS_H
