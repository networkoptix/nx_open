#ifndef TABLE_VIEW_H
#define TABLE_VIEW_H

#include <QtGui/QTableView>

/**
 * This class fixes a bug in <tt>QTableView</tt> related to editor triggers.
 *
 */
class QnTableView : public QTableView
{
    Q_OBJECT

    typedef QTableView base_type;
public:
    explicit QnTableView(QWidget *parent = 0);
    virtual ~QnTableView();

protected:
    virtual bool edit(const QModelIndex &index, EditTrigger trigger, QEvent *event) override;

};

#endif // TABLE_VIEW_H
