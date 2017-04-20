#pragma once

#include <QtCore/QCoreApplication>

#define QN_DECLARE_TR_FUNCTIONS(context) \
static inline QString tr(const char *sourceText, const char *disambiguation = nullptr, int n = -1) \
{ return QCoreApplication::translate(context, sourceText, disambiguation, n); }
