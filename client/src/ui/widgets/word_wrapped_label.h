#ifndef WORD_WRAPPED_LABEL_H
#define WORD_WRAPPED_LABEL_H

#include <QtWidgets/QLabel>

class QnWordWrappedLabel : public QWidget {
    Q_OBJECT
    Q_PROPERTY(QString text READ text WRITE setText)
public:
    QnWordWrappedLabel(QWidget *parent = 0);

    QLabel* label() const;

    virtual QSize sizeHint() const override;
    virtual QSize minimumSizeHint() const override;

    QString text() const;
public Q_SLOTS:
    void setText(const QString &value);
protected:
    virtual void resizeEvent(QResizeEvent * event) override;
private:
    QLabel* m_label;
};

#endif // WORD_WRAPPED_LABEL_H