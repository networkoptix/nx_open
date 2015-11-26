
#define QN_USE_QT_STRING_LITERALS
#ifdef QN_USE_QT_STRING_LITERALS
namespace QnLitDetail { template<int N> void check_string_literal(const char(&)[N]) {} }
#   define lit(s) (QnLitDetail::check_string_literal(s), QStringLiteral(s))
#else
#   define lit(s) QLatin1String(s)
#endif
