#ifndef _MUSTACHE_MUSTACHE_HELPER_H_
#define _MUSTACHE_MUSTACHE_HELPER_H_



#include "mustache.h"

bool renderTemplateFromFile(
    const QString& filename,
    const QVariantMap& contextMap,
    QString* const renderedMail );


#endif // _MUSTACHE_MUSTACHE_HELPER_H_