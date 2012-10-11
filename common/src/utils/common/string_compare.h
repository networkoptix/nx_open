/*
 * This software was written by people from OnShore Consulting services LLC
 * <info@sabgroup.com> and placed in the public domain.
 *
 * We reserve no legal rights to any of this. You are free to do
 * whatever you want with it. And we make no guarantee or accept
 * any claims on damages as a result of this.
 *
 * If you change the software, please help us and others improve the
 * code by sending your modifications to us. If you choose to do so,
 * your changes will be included under this license, and we will add
 * your name to the list of contributors.
 */
#ifndef QN_STRING_COMPARE_H
#define QN_STRING_COMPARE_H

#include <QtCore/QString>
#include <QtCore/QStringList>

int qnNaturalStringCompare(const QString &lhs, const QString &rhs, Qt::CaseSensitivity caseSensitive = Qt::CaseSensitive);
QStringList qnNaturalStringSort(const QStringList &list, Qt::CaseSensitivity caseSensitive = Qt::CaseSensitive);

bool qnNaturalStringLessThan(const QString &lhs, const QString &rhs);
bool qnNaturalStringCaseInsensitiveLessThan(const QString &lhs, const QString &rhs);

#endif // QN_STRING_COMPARE_H

