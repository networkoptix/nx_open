// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QHash>
#include <QtCore/QString>
#include <QtGui/QIcon>
#include <QtGui/QMovie>
#include <QtGui/QPixmap>

#include "icon.h"
#include "svg_icon_colorer.h"

class QStyle;
class QSvgRenderer;

namespace nx::vms::client::core {

class IconLoader;
class GenericPalette;

class NX_VMS_CLIENT_CORE_API Skin: public QObject
{
    Q_OBJECT

public:
    Skin(const QStringList& paths, QObject* parent = nullptr);
    virtual ~Skin() override;

    static Skin* instance();

    const QStringList& paths() const;

    QString path(const QString& name) const;
    QString path(const char* name) const;

    bool hasFile(const QString& name) const;
    bool hasFile(const char* name) const;

    QIcon icon(const QString& name,
        const QString& checkedName = QString(),
        const QnIcon::Suffixes* suffixes = nullptr,
        const SvgIconColorer::IconSubstitutions& svgColorSubstitutions = {},
        const SvgIconColorer::IconSubstitutions& svgCheckedColorSubstitutions = {},
        const SvgIconColorer::ThemeSubstitutions& svgThemeSubstitutions = {});
    QIcon icon(const char* name, const char* checkedName = nullptr);
    QIcon icon(const QIcon& icon);
    QIcon icon(const QString& name,
        const SvgIconColorer::IconSubstitutions& svgColorSubstitutions,
        const SvgIconColorer::IconSubstitutions& svgCheckedColorSubstitutions = {});
    QIcon icon(const QString& name,
        const SvgIconColorer::IconSubstitutions& svgColorSubstitutions,
        const QString& checkedName,
        const SvgIconColorer::IconSubstitutions& svgCheckedColorSubstitutions = {});
    QIcon icon(const QString& name,
        const SvgIconColorer::ThemeSubstitutions& themeSubstitutions);
    QIcon icon(const QString& name,
        const SvgIconColorer::ThemeSubstitutions& themeSubstitutions,
        const QString& checkedName);

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

} // namespace nx::vms::client::core

#define qnSkin (nx::vms::client::core::Skin::instance())
