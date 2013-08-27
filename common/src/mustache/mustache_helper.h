#ifndef _MUSTACHE_MUSTACHE_HELPER_H_
#define _MUSTACHE_MUSTACHE_HELPER_H_

#include "mustache/mustache.h"

QString renderTemplateFromFile(const QString& path, const QString& filename, const QVariantHash& contextMap);

#endif // _MUSTACHE_MUSTACHE_HELPER_H_