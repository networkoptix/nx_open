// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QList>

class TabWidthCalculator
{
public:
    TabWidthCalculator() = default;
    void calculate(int desiredWidth);
    void clear();
    void addTab(int width);
    void setWidth(int index, int width);
    void moveTab(int from, int to);
    void reset();
    int count() const;
    int width(int index) const;

private:
    struct WidthInfo
    {
        int width = 0;
        int count = 0;
    };
    QList<WidthInfo>::iterator findOrderedBound(int width);
    void addOrderedWidth(int width);

private:
    QList<int> m_tabWidths;
    QList<int> m_tabOriginalWidths;
    QList<WidthInfo> m_orderedTabWidths;
    int m_sumTabWidths;
};
