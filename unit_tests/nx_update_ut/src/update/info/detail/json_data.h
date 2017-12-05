#pragma once

#include <vector>

struct UpdateTestData
{
    QString customization;
    QString version;
    QByteArray json;
};


extern const char* const metaDataJson;
extern const std::vector<UpdateTestData> updateTestDataList;
