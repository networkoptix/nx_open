// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QHeaderView>

namespace nx::vms::client::desktop {

class CheckableHeaderView: public QHeaderView
{
    Q_OBJECT
    typedef QHeaderView base_type;

public:
    explicit CheckableHeaderView(int checkboxColumn, QWidget* parent = nullptr);

    Qt::CheckState checkState() const;
    void setCheckState(Qt::CheckState state);

    bool highlightCheckedIndicator() const;
    void setHighlightCheckedIndicator(bool value);

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
    const int m_checkBoxColumn;
    bool m_highlightCheckedIndicator = false;
    Qt::CheckState m_checkState = Qt::Unchecked;
};

} // namespace nx::vms::client::desktop
