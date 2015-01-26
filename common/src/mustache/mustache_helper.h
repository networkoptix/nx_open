#ifndef _MUSTACHE_MUSTACHE_HELPER_H_
#define _MUSTACHE_MUSTACHE_HELPER_H_

#ifdef ENABLE_SENDMAIL

#include "mustache/mustache.h"

QString renderTemplateFromFile(const QString& filename, const QVariantHash& contextMap);

#endif // ENABLE_SENDMAIL

#endif // _MUSTACHE_MUSTACHE_HELPER_H_
