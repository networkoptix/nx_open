#include "histogram_widget.h"

#include <QtGui/QPainter>
#include <QtGui/QPen>

#include <utils/math/linear_combination.h>

#include <ui/style/globals.h>

QnHistogramWidget::QnHistogramWidget(QWidget *parent, Qt::WindowFlags windowFlags):
    base_type(parent, windowFlags)
{}

const QnHistogramColors &QnHistogramWidget::colors() const {
    return m_colors;
}

void QnHistogramWidget::setColors(const QnHistogramColors &colors) {
    m_colors = colors;
    update();
}

void QnHistogramWidget::setHistogramData(const ImageCorrectionResult& data)
{
    if (data.filled) {
        m_data = data;
        update();
    }
}

void QnHistogramWidget::setHistogramParams(const ImageCorrectionParams& params)
{
    m_params = params;
    update();
}

void QnHistogramWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    
    painter.setPen(Qt::NoPen);
    painter.setBrush(m_colors.background);
    painter.drawRect(rect());

    double w = width();
    if (isEnabled() && m_params.enabled)
    {
        const int *data = m_data.hystogram;
        double yScale = INT_MAX;
        for (int i = 0; i < 256; ++i)
            yScale = qMin(yScale, height() / (double)data[i]);

        int prevY = 0;

        QVector<QLine> lines;
        QVector<QLine> lines2;

        for (int x = 0; x < width(); ++x)
        {
            double idx = x / w * 256.0;
            quint8 index = idx;
            quint8 index2 = index < 255 ? index+1 : 255;
            double idxFraq = (idx - index) * 100;
            double value = (data[index] * (100 - idxFraq) + data[index2] * idxFraq) / 100.0;


            int curY = height() - value * yScale + 0.5;
            lines << QLine(x, height(), x, curY);
            if (x > 0)
                lines2 << QLine(x - 1, prevY - 1, x - 1, curY);
            prevY = curY;
        }

        painter.setPen(m_colors.histogram);
        painter.drawLines(lines);
        painter.setPen(linearCombine(0.5, m_colors.histogram, 0.5, m_colors.background));
        painter.drawLines(lines2);

        painter.setPen(m_colors.selection.lighter());
        painter.setBrush(m_colors.selection);
        double xScale = w / 256;
        painter.drawRect(QRect(qAbs(m_data.bCoeff * 256.0) * xScale, 1,  256.0 / m_data.aCoeff * xScale + 0.5, height()));
        
        painter.setPen(m_colors.text);
        painter.drawText(QRect(2, 2, width() - 4, height() - 4), Qt::AlignRight, tr("Gamma %1").arg(m_data.gamma, 0, 'f', 2));
    }

    QPen pen;
    pen.setStyle(Qt::DashLine);
    pen.setWidth(1);
    pen.setBrush(m_colors.grid);
    painter.setPen(pen);
    painter.drawLine(width() / 4, 0, width() / 4, height());
    painter.drawLine(width() / 2, 0, width() / 2, height());
    painter.drawLine(width() / 4 * 3, 0, width() / 4 * 3, height());
    painter.drawLine(0, height() / 2, width(), height() / 2);

    painter.setBrush(Qt::NoBrush);
    painter.setPen(m_colors.border);
    painter.drawRect(QRect(0, 0, width() - 1, height() - 1));
}
