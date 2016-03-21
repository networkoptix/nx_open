#ifndef QN_UNUSED_H
#define QN_UNUSED_H

namespace QnUnusedDetail {
  inline void mark_unused() {}

  template<class T0>
  inline void mark_unused(const T0 &) {}

  template<class T0, class T1>
  inline void mark_unused(const T0 &, const T1 &) {}

  template<class T0, class T1, class T2>
  inline void mark_unused(const T0 &, const T1 &, const T2 &) {}

  template<class T0, class T1, class T2, class T3>
  inline void mark_unused(const T0 &, const T1 &, const T2 &, const T3 &) {}

  template<class T0, class T1, class T2, class T3, class T4>
  inline void mark_unused(const T0 &, const T1 &, const T2 &, const T3 &, const T4 &) {}

} // namespace QnUnusedDetail

/**
 * Macro to suppress unused parameter warnings. Unlike constructions like 
 * <tt>Q_UNUSED(X)</tt> or <tt>(void)x</tt>, does not require the parameter
 * to have a complete type.
 * 
 * \param ...                          Comma-separated list of parameters to 
 *                                     mark as intentionally unused.
 */
#define QN_UNUSED(...) ::QnUnusedDetail::mark_unused(__VA_ARGS__)

#endif // QN_UNUSED_H
