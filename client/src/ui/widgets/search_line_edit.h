#ifndef SEARCH_LINE_EDIT_H
#define SEARCH_LINE_EDIT_H

#include <QtWidgets/QLineEdit>

class QnSearchLineEdit : public QWidget
{
    Q_OBJECT
public:
    explicit QnSearchLineEdit(QWidget *parent = 0);

    inline QLineEdit *lineEdit() const { return m_lineEdit; }

    QSize sizeHint() const;

    QVariant inputMethodQuery(Qt::InputMethodQuery property) const;

signals:
    void textChanged(const QString &text);
    void escKeyPressed();
    void enterKeyPressed();
    void enabledChanged();

public slots:
    void updateFoundItemsCount(int count);
    void updateOverallItemsCount(int count);

protected:
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void inputMethodEvent(QInputMethodEvent *e) override;
    bool event(QEvent *event) override;
    void changeEvent(QEvent *event);

protected:
    void updateGeometries();
    void initStyleOption(QStyleOptionFrameV2 *option) const;

    QLineEdit* m_lineEdit;
    QAbstractButton *m_closeButton;
    QLabel *m_occurencesLabel;
    QAbstractButton *m_searchButton;

    int m_foundItems;
    int m_overallItems;
};

#endif // SEARCH_LINE_EDIT_H
