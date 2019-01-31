#pragma once

#include <vector>

#include <nx/fusion/model_functions_fwd.h>

struct QnMacAndDeviceClass {
    QnMacAndDeviceClass() {}

    QnMacAndDeviceClass(const QString &_class, const QString &_mac)
        : xclass(_class),
          mac(_mac)
    {}

    QString xclass;
    QString mac;
};

#define QnMacAndDeviceClass_Fields (xclass)(mac)
QN_FUSION_DECLARE_FUNCTIONS(QnMacAndDeviceClass, (json))

typedef std::vector<QnMacAndDeviceClass> QnMacAndDeviceClassList;

struct QnHardwareInfo  {
    QString boardID;
    QString boardUUID;
    QString compatibilityBoardUUID;
    QString boardManufacturer;
    QString boardProduct;

    QString biosID;
    QString biosManufacturer;

    QString memoryPartNumber;
    QString memorySerialNumber;

    QnMacAndDeviceClassList nics;
    QString mac;

    QString date;
};

#define QnHardwareInfo_Fields (date)(boardID)(boardUUID)(compatibilityBoardUUID)(boardManufacturer)(boardProduct)(biosID)(biosManufacturer)(memoryPartNumber)(memorySerialNumber)(nics)(mac)
QN_FUSION_DECLARE_FUNCTIONS(QnHardwareInfo, (json))