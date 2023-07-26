// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QHash>
#include <QtGui/QIcon>
#include <QtGui/QMovie>
#include <QtGui/QPixmap>

#include <nx/utils/singleton.h>

#include "icon.h"
#include "svg_icon_colorer.h"

class QStyle;
class QSvgRenderer;

namespace nx::vms::client::desktop {

class IconLoader;
class GenericPalette;

class NX_VMS_CLIENT_DESKTOP_API Skin: public QObject, public Singleton<Skin>
{
    Q_OBJECT

public:
    Skin(const QStringList& paths, QObject* parent = nullptr);
    virtual ~Skin();

    const QStringList& paths() const;

    QString path(const QString& name) const;
    QString path(const char* name) const;

    bool hasFile(const QString& name) const;
    bool hasFile(const char* name) const;


    QIcon icon(const QString& name,
        const QString& checkedName = QString(),
        const QnIcon::Suffixes* suffixes = nullptr,
        const SvgIconColorer::IconSubstitutions& svgColorSubstitutions =
            SvgIconColorer::kDefaultIconSubstitutions);
    QIcon icon(const char* name, const char* checkedName = nullptr);
    QIcon icon(const QIcon& icon);

    /**
     * Loads pixmap with appropriate size according to current hidpi settings.
     * @param desiredSize is used for svg images only, this size may be corrected internally
     *     if `correctDevicePixelRatio` is set to true.
     **/
    QPixmap pixmap(
        const QString& name,
        bool correctDevicePixelRatio = true,
        const QSize& desiredSize = QSize());

    QMovie* newMovie(const QString& name, QObject* parent = nullptr);
    QMovie* newMovie(const char* name, QObject* parent = nullptr);

    static QSize maximumSize(const QIcon& icon, QIcon::Mode mode = QIcon::Normal,
        QIcon::State state = QIcon::Off);

    static QPixmap maximumSizePixmap(const QIcon& icon, QIcon::Mode mode = QIcon::Normal,
        QIcon::State state = QIcon::Off, bool correctDevicePixelRatio = true);

    static QStyle* newStyle();

    static bool isHiDpi();

    static QPixmap colorize(const QPixmap& source, const QColor& color);

private:
    void init(const QStringList& paths);

    bool initSvgRenderer(const QString& name, QSvgRenderer& renderer) const;
    QPixmap getPixmapInternal(const QString& name);
    QPixmap getPixmapFromSvg(const QString& name, bool correctDevicePixelRatio, const QSize& desiredSize);
    QString getDpiDependedName(const QString& name) const;

private:
    QStringList m_paths;
    IconLoader* m_iconLoader;
};

} // namespace nx::vms::client::desktop

#define qnSkin (::nx::vms::client::desktop::Skin::instance())
