#pragma once

/**
 * Utility method to paint pixmap on the painter, leaving it sharp.
 * Actual if the bitmap contains text or small details.
 */
void paintPixmapSharp( QPainter *painter, const QPixmap &pixmap, const QPointF &position = QPointF());
