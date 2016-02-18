
#pragma once

#include <api/helpers/multiserver_request_data.h>

// @brief Represents request without data. We need it
// because we can't create QnMultiserverRequestData directly
class QnEmptyRequestData : public QnMultiserverRequestData
{
public:
    QnEmptyRequestData();
};
