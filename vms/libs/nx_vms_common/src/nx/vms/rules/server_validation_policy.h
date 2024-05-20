// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QCoreApplication> //for Q_DECLARE_TR_FUNCTIONS

#include <core/resource/resource_fwd.h>

class NX_VMS_COMMON_API QnBuzzerPolicy
{
    Q_DECLARE_TR_FUNCTIONS(QnBuzzerPolicy)

public:
    static bool isServerValid(const QnMediaServerResourcePtr& server);
    static QString infoText();
};

class NX_VMS_COMMON_API QnPoeOverBudgetPolicy
{
    Q_DECLARE_TR_FUNCTIONS(QnPoeOverBudgetPolicy)

public:
    static bool isServerValid(const QnMediaServerResourcePtr& server);
    static QString infoText();
};

class NX_VMS_COMMON_API QnFanErrorPolicy
{
    Q_DECLARE_TR_FUNCTIONS(QnFanErrorPolicy)

public:
    static bool isServerValid(const QnMediaServerResourcePtr& server);
    static QString infoText();
};
