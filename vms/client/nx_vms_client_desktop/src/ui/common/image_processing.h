#pragma once

#include <QtGui/QImage>

class QnAspectRatio;

void gaussianBlur(const QImage &src, qreal sigma, QImage *dst);

QImage gaussianBlur(const QImage &src, qreal sigma);

QImage cropToAspectRatio(const QImage &source, const QnAspectRatio& targetAspectRatio);
QImage cropToAspectRatio(const QImage &source, const qreal targetAspectRatio);
