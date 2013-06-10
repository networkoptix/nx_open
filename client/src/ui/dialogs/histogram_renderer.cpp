#include "histogram_renderer.h"
#include "ui/style/globals.h"

static const int LINE_OFFSET = 4;
static const int X_OFFSET = 8;

QnHistogramRenderer::QnHistogramRenderer(QWidget* parent):
    QWidget(parent),
    m_aCoeff(1.0),
    m_bCoeff(0.0)
{

}

void QnHistogramRenderer::setHistogramData(const QByteArray& data, double aCoeff, double bCoeff)
{
    m_histogramData = data;
    m_aCoeff = aCoeff;
    m_bCoeff = bCoeff*256.0;
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

    const int* data = (const int*) m_histogramData.constData();
    double yScale = INT_MAX;
    for (int i = 0; i < 256; ++i)
        yScale = qMin(yScale, height() / (double)data[i]);

    double w = width() - X_OFFSET*2;
    int prevY = 0;
    for (int x = 0; x < width() - X_OFFSET*2; ++x)
    {
        p.setPen(Qt::white);
        double idx = x / w * 256.0;
        quint8 index = idx;
        quint8 index2 = index < 255 ? index+1 : 255;
        double idxFraq = (idx - index) * 100;
        double value = (data[index]*(100-idxFraq) + data[index2]*idxFraq) / 100.0;


        int curY = height() - value * yScale + 0.5;
        p.drawLine(X_OFFSET + x, height(), X_OFFSET + x, curY);
        if (x > X_OFFSET) {
            p.setPen(QColor(0x80, 0x80, 0x80, 0x80));
            p.drawLine(X_OFFSET + x-1, prevY-1, X_OFFSET + x-1, curY);
        }
        prevY = curY;
    }

    const QColor selectionColor = qnGlobals->selectionColor();
    p.setPen(selectionColor.lighter());
    p.setBrush(selectionColor);
    double xScale = w / 256;
    p.drawRect(QRect(qAbs(m_bCoeff)*xScale + X_OFFSET, 1,  256.0/m_aCoeff*xScale+0.5, height()));
}
