// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/


#pragma once

#include <api/helpers/multiserver_request_data.h>

// @brief Represents request without data. We need it
// because we can't create QnMultiserverRequestData directly
class NX_VMS_COMMON_API QnEmptyRequestData: public QnMultiserverRequestData
{
public:
    QnEmptyRequestData();
};
