#pragma once

#include <vector>

#include <QString>

#include <nx/core/layout/layout_file_info.h>

struct LayoutStreamInfo
{
    QString name;
    bool isCrypted = false;
    qint64 position;
    qint64 size;
    qint64 grossSize;
    bool isOK = true;
};

struct LayoutStructure
{
    QString fileName;
    nx::core::layout::FileInfo info;

    bool isExe = false;

    std::vector<LayoutStreamInfo> streams;

    QString errorText;
};