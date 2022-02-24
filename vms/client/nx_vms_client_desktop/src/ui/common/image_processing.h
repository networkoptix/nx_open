// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtGui/QImage>

class QnAspectRatio;

void gaussianBlur(const QImage &src, qreal sigma, QImage *dst);

QImage gaussianBlur(const QImage &src, qreal sigma);

QImage cropToAspectRatio(const QImage &source, const QnAspectRatio& targetAspectRatio);
QImage cropToAspectRatio(const QImage &source, const qreal targetAspectRatio);
