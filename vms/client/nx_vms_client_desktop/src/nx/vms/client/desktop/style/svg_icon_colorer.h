// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QDir>
#include <QtCore/QMap>
#include <QtCore/QString>
#include <QtGui/QColor>
#include <QtGui/QIcon>

namespace nx::vms::client::desktop {

class NX_VMS_CLIENT_DESKTOP_API SvgIconColorer
{
public:
    using Substitutions = QMap<QColor, QString>;

    class IconSubstitutions: public QMap<QIcon::Mode, Substitutions>
    {
    public:
        using QMap<QIcon::Mode, Substitutions>::QMap;

        /**
         * Can be used to easily pass kDefaultIconSubstitutions appended by custom substitutions
         * into constructor.
         */
        IconSubstitutions operator+(const IconSubstitutions& other) const;
    };

    static QByteArray colorized(const QByteArray& source);

    /**
     * @param sourceIconData Original icon SVG file content.
     * @param iconName Icon file name.
     * @param substitutions Per-mode map of color substitutions required to make icons for modes
     *     other than QIcon::Normal. Substitutions::T values (QString) must be names of
     *     color theme colors (i.e. "#xxxxxx" form is not allowed).
     */
    SvgIconColorer(
        const QByteArray& sourceIconData,
        const QString& iconName,
        const IconSubstitutions& substitutions = kDefaultIconSubstitutions);

    /**
     * If mode is QIcon::Normal, then applies color theme color substitutions. Otherwise applies
     * substitutions provided for constructor depending on mode specified.
     * @return SVG file content with color values modified where needed.
     */
    QByteArray makeIcon(QIcon::Mode mode) const;

    static const IconSubstitutions kDefaultIconSubstitutions;

private:
    void initializeDump();
    void dumpIconIfNeeded(const QByteArray& data, const QString& suffix = QString()) const;

private:
    bool m_dump = false;
    QDir m_dumpDirectory;
    const QByteArray m_sourceIconData;
    QString m_iconName;
    const IconSubstitutions m_substitutions;
};

} // namespace nx::vms::client::desktop
