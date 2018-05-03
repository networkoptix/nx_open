#pragma once

#include <QtCore/QSharedPointer>

class QnLicense;
using QnLicensePtr = QSharedPointer<QnLicense>;
using QnLicenseList = QList<QnLicensePtr> ;
using QnLicenseDict = QMap<QByteArray, QnLicensePtr>;

class QnLicenseValidator;

enum class QnLicenseErrorCode;

enum class QnLicenseUsageStatus;
