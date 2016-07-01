#pragma once

#include <QtWidgets/QHeaderView>

class QnCheckBoxedHeaderView: public QHeaderView
{
    Q_OBJECT
    typedef QHeaderView base_type;

public:
    explicit QnCheckBoxedHeaderView(int checkboxColumn, QWidget* parent = nullptr);

    Qt::CheckState checkState() const;
    void setCheckState(Qt::CheckState state);

signals:
    void checkStateChanged(Qt::CheckState state);

protected:
    virtual void paintSection(QPainter* painter, const QRect& rect, int logicalIndex) const override;
    virtual QSize sectionSizeFromContents(int logicalIndex) const override;
    virtual void mousePressEvent(QMouseEvent* event) override;
    virtual void mouseReleaseEvent(QMouseEvent* event) override;

private slots:
    void at_sectionClicked(int logicalIndex);

private:
    int m_checkBoxColumn;
    Qt::CheckState m_checkState;
};
