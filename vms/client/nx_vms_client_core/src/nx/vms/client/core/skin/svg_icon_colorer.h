// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <sstream>

#include <QtCore/QByteArray>
#include <QtCore/QDir>
#include <QtCore/QMap>
#include <QtCore/QString>
#include <QtGui/QColor>
#include <QtGui/QIcon>

namespace nx::vms::client::core {

static const QString kInvalidColor = "invalidColor";


class NX_VMS_CLIENT_CORE_API SvgIconColorer
{
public:
    struct ThemeColorsRemapData
    {
        QString primary = kInvalidColor;
        QString secondary = kInvalidColor;
        QString tertiary = kInvalidColor;
        double alpha = 1.0;

        bool contains(QString name) const
        {
            return name == "primary" || name == "secondary" || name == "tertiary";
        }

        bool empty() const
        {
            return primary == kInvalidColor
                && secondary == kInvalidColor
                && tertiary == kInvalidColor;
        }

        const QString operator[](QString name) const
        {
            if (name == "primary")
                return primary;
            if (name == "secondary")
                return secondary;
            if (name == "tertiary")
                return tertiary;
            return kInvalidColor;
        }

        QString toString() const
        {
            std::stringstream ss;
            ss << primary.toStdString() << "-" << secondary.toStdString() << "-"
               << tertiary.toStdString() << "-" << alpha;
            return QString::fromStdString(ss.str());
        }
    };

    struct ThemeSubstitutions: public QMap<QIcon::Mode, ThemeColorsRemapData>
    {
        using QMap<QIcon::Mode, ThemeColorsRemapData>::QMap;
        QString hash() const;
    };

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

        QString hash() const;
    };

    static QByteArray colorized(const QByteArray& source);

    /**
     * @param sourceIconData Original icon SVG file content.
     * @param iconName Icon file name.
     * @param substitutions // TODO @pprivalov Should be removed when all icons transformed to use new notation
     *     Per-mode map of color substitutions required to make icons for modes
     *     other than QIcon::Normal. Substitutions::T values (QString) must be names of
     *     color theme colors (i.e. "#xxxxxx" form is not allowed).
     * @param themeSubstitutions Per-mode map of color substitutions required to make icons.
     *     Substitutions::T values (QString) must be names of
     *     color theme colors (i.e. "#xxxxxx" form is not allowed).
     */
    SvgIconColorer(const QByteArray& sourceIconData,
        const QString& iconName,
        const IconSubstitutions& substitutions = kDefaultIconSubstitutions,
        const QMap<QIcon::Mode, ThemeColorsRemapData>& themeSubstitutions = {});

    /**
     * If mode is QIcon::Normal, then applies color theme color substitutions. Otherwise applies
     * substitutions provided for constructor depending on mode specified.
     * @return SVG file content with color values modified where needed.
     */
    QByteArray makeIcon(QIcon::Mode mode) const;

    static const IconSubstitutions kDefaultIconSubstitutions;
    static const IconSubstitutions kTreeIconSubstitutions;

private:
    void initializeDump();
    void dumpIconIfNeeded(const QByteArray& data, const QString& suffix = QString()) const;

private:
    bool m_dump = false;
    QDir m_dumpDirectory;
    const QByteArray m_sourceIconData;
    QString m_iconName;
    const IconSubstitutions m_substitutions;
    QMap<QIcon::Mode, ThemeColorsRemapData> m_themeSubstitutions;
};

} // namespace nx::vms::client::core
