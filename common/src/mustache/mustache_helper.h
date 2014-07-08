#ifndef _MUSTACHE_MUSTACHE_HELPER_H_
#define _MUSTACHE_MUSTACHE_HELPER_H_

#ifdef ENABLE_MUSTACHE

#include "mustache/mustache.h"

QString renderTemplateFromFile(const QString& path, const QString& filename, const QVariantHash& contextMap);

#endif // ENABLE_MUSTACHE

#endif // _MUSTACHE_MUSTACHE_HELPER_H_
