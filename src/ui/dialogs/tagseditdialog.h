#ifndef TAGSEDITDIALOG_H
#define TAGSEDITDIALOG_H

#include <QtGui/QDialog>

class QSortFilterProxyModel;
class QStringListModel;

namespace Ui {
    class TagsEditDialog;
}

class TagsEditDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TagsEditDialog(const QString &objectId, QWidget *parent = 0);
    ~TagsEditDialog();

public Q_SLOTS:
    virtual void accept();
    virtual void reset();

protected:
    void changeEvent(QEvent *e);
    void keyPressEvent(QKeyEvent *e);

private Q_SLOTS:
    void addTag();
    void addTags();
    void removeTags();

    void filterChanged(const QString &filter);

private:
    void addTags(const QStringList &tags);
    void removeTags(const QStringList &tags);

private:
    Q_DISABLE_COPY(TagsEditDialog)

    Ui::TagsEditDialog *ui;

    QString m_objectId;

    QStringListModel *m_objectTagsModel;
    QStringListModel *m_allTagsModel;
    QSortFilterProxyModel *m_allTagsProxyModel;
};

#endif // TAGSEDITDIALOG_H
