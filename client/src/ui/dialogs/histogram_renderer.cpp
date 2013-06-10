#include "histogram_renderer.h"
#include "ui/style/globals.h"

static const int LINE_OFFSET = 4;
static const int X_OFFSET = 8;

QnHistogramRenderer::QnHistogramRenderer(QWidget* parent):
    QWidget(parent)
{

}

void QnHistogramRenderer::setHistogramData(const ImageCorrectionResult& data)
{
    m_data = data;
    update();
}

void QnHistogramRenderer::setHistogramParams(const ImageCorrectionParams& params)
{
    m_params = params;
    update();
}

void QnHistogramRenderer::paintEvent( QPaintEvent * event )
{
    QPainter p(this);
    
    p.setBrush(QBrush(Qt::Dense5Pattern));
    p.drawRect(QRect(X_OFFSET, 0, width()-X_OFFSET*2, height()-1));

    const int* data = m_data.hystogram;
    double yScale = INT_MAX;
    for (int i = 0; i < 256; ++i)
        yScale = qMin(yScale, height() / (double)data[i]);

    double w = width() - X_OFFSET*2;
    int prevY = 0;

    QVector<QLine> lines;
    QVector<QLine> lines2;

    for (int x = 0; x < width() - X_OFFSET*2; ++x)
    {
        double idx = x / w * 256.0;
        quint8 index = idx;
        quint8 index2 = index < 255 ? index+1 : 255;
        double idxFraq = (idx - index) * 100;
        double value = (data[index]*(100-idxFraq) + data[index2]*idxFraq) / 100.0;


        int curY = height() - value * yScale + 0.5;
        lines << QLine(X_OFFSET + x, height(), X_OFFSET + x, curY);
        if (x > X_OFFSET) {
            lines2 << QLine(X_OFFSET + x-1, prevY-1, X_OFFSET + x-1, curY);
        }
        prevY = curY;
    }

    p.setPen(Qt::white);
    p.drawLines(lines);
    p.setPen(QColor(0x60, 0x60, 0x60));
    p.drawLines(lines2);


    QPen pen;
    pen.setStyle(Qt::DashLine);
    pen.setWidth(1);
    pen.setBrush(QColor(0,255,0, 30));
    p.setPen(pen);
    p.drawLine(width()/4, 0, width()/4, height());
    p.drawLine(width()/2, 0, width()/2, height());
    p.drawLine(width()/4*3, 0, width()/4*3, height());
    p.drawLine(X_OFFSET, height()/2, width()-X_OFFSET, height()/2);
    //p.drawLine(X_OFFSET, height()/4, width()-X_OFFSET, height()/4);
    //p.drawLine(X_OFFSET, height()/4*3, width()-X_OFFSET, height()/4*3);
    

    const QColor selectionColor = qnGlobals->selectionColor();
    p.setPen(selectionColor.lighter());
    p.setBrush(selectionColor);
    double xScale = w / 256;
    p.drawRect(QRect(qAbs(m_data.bCoeff*256.0)*xScale + X_OFFSET, 1,  256.0/m_data.aCoeff*xScale+0.5, height()));
    
    p.setPen(Qt::white);
    QRect r(0,0, width() - X_OFFSET*2, height());
    p.drawText(r, Qt::AlignRight, tr("Gamma %1").arg(m_data.gamma, 0, 'f', 2));
}
