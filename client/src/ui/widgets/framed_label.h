#ifndef FRAMED_LABEL_H
#define FRAMED_LABEL_H

#include <QLabel>

class QnFramedLabel: public QLabel {
    typedef QLabel base_type;
public:
    explicit QnFramedLabel(QWidget* parent = 0);
    virtual ~QnFramedLabel();

    QSize size() const;

    int opacityPercent() const;

    void setOpacityPercent(int value);

    void debugSize();

protected:
    virtual void resizeEvent(QResizeEvent *event) override;
    virtual void QnFramedLabel::paintEvent(QPaintEvent *event) override;

private:
    int m_opacityPercent;
};

#endif // FRAMED_LABEL_H
