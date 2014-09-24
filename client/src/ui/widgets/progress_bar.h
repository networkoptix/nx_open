#ifndef QN_PROGRESS_BAR_H
#define QN_PROGRESS_BAR_H

#include <QtWidgets/QProgressBar>

class QnStyleOptionProgressBar;

class QnProgressBar : public QProgressBar {
    Q_OBJECT

    typedef QProgressBar base_type;
public:
    QnProgressBar(QWidget *parent = 0);

    QList<int> separators() const;
    void setSeparators(const QList<int> &separators);

protected:
    virtual void initStyleOption(QnStyleOptionProgressBar *option) const;

    virtual void paintEvent(QPaintEvent *event) override;

private:
    QList<int> m_separators;
};

#endif // QN_PROGRESS_BAR_H
