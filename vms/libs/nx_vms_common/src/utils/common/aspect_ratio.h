// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QSize>

class NX_VMS_COMMON_API QnAspectRatio
{
public:
    QnAspectRatio();
    QnAspectRatio(int width, int height);
    explicit QnAspectRatio(const QSize& size);

    bool isValid() const;

    int width() const;
    int height() const;

    /** @returns inverted (1/x) aspect ratio */
    QnAspectRatio inverted() const;

    QnAspectRatio tiled(const QSize& layout) const;

    float toFloat() const;
    QString toString() const;
    QString toString(const QString &format) const;

    static QList<QnAspectRatio> standardRatios();
    static QnAspectRatio closestStandardRatio(float aspectRatio);
    static const int kInvalidAspectRatio = -1;

    static bool isRotated90(qreal angle);

    bool operator==(const QnAspectRatio& other) const;
    bool operator!=(const QnAspectRatio& other) const;

private:
    int m_width = 0;
    int m_height = 0;
};
