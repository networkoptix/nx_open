// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QSharedPointer>

class QnLicense;
using QnLicensePtr = QSharedPointer<QnLicense>;
using QnLicenseList = QList<QnLicensePtr>;
using QnLicenseDict = QMap<QByteArray, QnLicensePtr>;
