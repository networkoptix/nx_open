#ifndef WORD_WRAPPED_LABEL_H
#define WORD_WRAPPED_LABEL_H

#include <QtWidgets/QLabel>

class QnWordWrappedLabel : public QWidget {
    Q_OBJECT
    Q_PROPERTY(QString text READ text WRITE setText)
    Q_PROPERTY(Qt::TextFormat textFormat READ textFormat WRITE setTextFormat)
public:
    QnWordWrappedLabel(QWidget *parent = 0);

    QLabel* label() const;

    virtual QSize sizeHint() const override;
    virtual QSize minimumSizeHint() const override;

    QString text() const;
    void setText(const QString &value);

    Qt::TextFormat textFormat() const;
    void setTextFormat(Qt::TextFormat value);

signals:
    void linkActivated(const QString &value);

protected:
    virtual void resizeEvent(QResizeEvent * event) override;
private:
    QLabel* m_label;
};

#endif // WORD_WRAPPED_LABEL_H