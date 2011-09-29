#if defined __cplusplus
#include <qglobal.h>
#ifdef Q_WS_WIN
# define _POSIX_
# include <limits.h>
# undef _POSIX_
#endif

#include <qlist.h>
#include <qvariant.h>  /* All moc genereated code has this include */
#include <qobject.h>
#include <qstring.h>
#include <qstringlist.h>
#include <qvector.h>
#include <qset.h>
#include <qhash.h>
#endif // __cplusplus
