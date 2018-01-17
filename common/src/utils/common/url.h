#ifndef QN_URL_H
#define QN_URL_H

#include <QtCore/QUrl>

inline bool qnUrlEqual(const nx::utils::Url &l, const nx::utils::Url &r, QUrl::FormattingOptions ignored = QUrl::RemovePassword | QUrl::RemoveScheme) {
    nx::utils::Url lc(l), rc(r);
    if (ignored & QUrl::RemovePassword) {
        lc.setPassword(QString());
        rc.setPassword(QString());
    }

    if (ignored & QUrl::RemoveScheme)
        lc.setScheme(rc.scheme());

    return lc == rc;
}


#endif // QN_URL_H
