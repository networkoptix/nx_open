#ifndef QN_MARGIN_FLAGS_H
#define QN_MARGIN_FLAGS_H

namespace Qn {
    enum MarginFlag {
        MARGINS_AFFECT_SIZE = 0x1,      /**< Viewport margins affect how viewport size is bounded. */
        MARGINS_AFFECT_POSITION = 0x2   /**< Viewport margins affect how viewport position is bounded. */
    };

    Q_DECLARE_FLAGS(MarginFlags, MarginFlag);
} // namespace Qn

Q_DECLARE_OPERATORS_FOR_FLAGS(Qn::MarginFlags);

#endif // QN_MARGIN_FLAGS_H
