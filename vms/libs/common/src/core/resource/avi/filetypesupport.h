#pragma once

#include <QtCore/QString>

class FileTypeSupport
{
public:
    static bool isFileSupported(const QString& filename);

    static bool isMovieFileExt(const QString& filename);
    static bool isImageFileExt(const QString& filename);
    static bool isValidLayoutFile(const QString& filename);
};
