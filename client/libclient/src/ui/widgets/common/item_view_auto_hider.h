#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

class QnItemViewAutoHiderPrivate;
class QAbstractItemView;
class QLabel;

/**
* An utility widget that takes ownership of a specified item view,
* shows it if it is not empty or hides it if it is empty
* and shows a text message instead:
*/
class QnItemViewAutoHider: public QWidget
{
    Q_OBJECT
    Q_PROPERTY(QAbstractItemView* view READ view WRITE setView)
    Q_PROPERTY(QString emptyMessage READ emptyMessage WRITE setEmptyMessage)
    Q_PROPERTY(QLabel* emptyMessageLabel READ emptyMessageLabel)
    using base_type = QWidget;

public:
    QnItemViewAutoHider(QWidget* parent = nullptr);
    virtual ~QnItemViewAutoHider();

    QAbstractItemView* view() const;
    void setView(QAbstractItemView* view);

    QString emptyMessage() const;
    void setEmptyMessage(const QString& message);

    QLabel* emptyMessageLabel() const;

private:
    Q_DECLARE_PRIVATE(QnItemViewAutoHider);
    QScopedPointer<QnItemViewAutoHiderPrivate> d_ptr;
};
