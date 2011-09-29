#ifndef serach_edit_item_h_2017
#define serach_edit_item_h_2017

#include <QtCore/QList>

#include <QtGui/QCompleter>
#include <QtGui/QLineEdit>

class GraphicsView;
class LayoutContent;

typedef QPair<QString, QString> StringPair;

class CLSearchEditCompleter : public QCompleter
{
    Q_OBJECT

public:
    CLSearchEditCompleter(QObject *parent);

    void filter(const QString &filter);

    void updateStringPairs(const QList<StringPair> &list);

private:
    QList<QPair<QString, QString> > m_list;
    QString m_word;
};


class CLSearchEditItem : public QWidget
{
    Q_OBJECT

public:
    CLSearchEditItem(GraphicsView *view, LayoutContent *sceneContent, QWidget *parent = 0);
    ~CLSearchEditItem();

    QLineEdit *lineEdit() const;

Q_SIGNALS:
    void keyPressed(); // for stupid workaround/hack

protected:
    bool eventFilter(QObject *watched, QEvent *event);

private Q_SLOTS:
    void onEditTextChanged(const QString &text);
    void onTimer();

    void onLiveButtonClicked();

private:
    GraphicsView *m_view;
    LayoutContent *m_sceneContent;

    QLineEdit *m_lineEdit;
    CLSearchEditCompleter *m_completer;
    QTimer mTimer;
};

#endif //serach_edit_item_h_2017
