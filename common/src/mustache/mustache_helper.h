#ifndef _MUSTACHE_MUSTACHE_HELPER_H_
#define _MUSTACHE_MUSTACHE_HELPER_H_

#include "mustache/mustache.h"

bool renderTemplateFromFile(
    const QString& filename,
    const QVariantHash& contextMap,
    QString* const renderedMail );

#endif // _MUSTACHE_MUSTACHE_HELPER_H_