// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <core/misc/screen_snap.h>

class QEvent;
class QScreen;
class QWidget;

class MultiscreenWidgetGeometrySetter final: public QObject
{
    using base_type = QObject;

public:
    explicit MultiscreenWidgetGeometrySetter(QWidget* target, QObject* parent = nullptr);

    void changeGeometry(const QnScreenSnaps& screenSnaps);

    bool eventFilter(QObject* watched, QEvent* event) final;

private:
    // This method works similar to QWindowPrivate::screenForGeometry and is needed to prevent
    // switching window screen before settings new geometry (that is probably a Qt bug).
    static QScreen* widgetScreenForGeometry(QWidget* widget, const QRect& newGeometry);

private:
    QWidget* const m_target;
    QnScreenSnaps m_screenSnaps;
    bool m_finished = true;
    int m_attempts = 0;
};
