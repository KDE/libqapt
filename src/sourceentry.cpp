/***************************************************************************
 *   Copyright © 2013 Jonathan Thomas <echidnaman@kubuntu.org>             *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or         *
 *   modify it under the terms of the GNU General Public License as        *
 *   published by the Free Software Foundation; either version 2 of        *
 *   the License or (at your option) version 3 or any later version        *
 *   accepted by the membership of KDE e.V. (or its successor approved     *
 *   by the membership of KDE e.V.), which shall act as a proxy            *
 *   defined in Section 14 of version 3 of the license.                    *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

#include "sourceentry.h"

// Qt includes
#include <QSharedData>
#include <QStringBuilder>
#include <QStringList>
#include <QDebug>
#include <QRegularExpression>

// APT includes
#include <apt-pkg/configuration.h>

namespace QApt {

class SourceEntryPrivate : public QSharedData
{
public:
    SourceEntryPrivate()
        : isValid(true)
        , isEnabled(true)
    {}

    SourceEntryPrivate(const QString &lineData, const QString &fileName)
        : isValid(true)
        , isEnabled(true)
        , line(lineData)
        , file(fileName)
    {
        if (file.isEmpty())
            file = QString::fromStdString(_config->FindFile("Dir::Etc::sourcelist"));

        parseData(line);
    }

    // Data members
    bool isValid;
    bool isEnabled;
    QString type;
    QStringList architectures;
    QString uri;
    QString dist;
    QStringList components;
    QString comment;
    QString line;
    QString file;

    void parseData(const QString &data);
};

void SourceEntryPrivate::parseData(const QString &data)
{
    if (data.isEmpty())
        return;

    QString tData = data.simplified();

    // Check for nonvalid input
    if (tData.isEmpty() || tData == QChar('#')) {
        isValid = false;
        return;
    }

    // Check source enable state
    if (tData.at(0) == '#') {
        isEnabled = false;
    }
    // Handle multiple comment characters (hey, it happens!)
    while (tData.size() > 0 && tData.at(0) == '#') {
        // Remove starting '#' from tData
        tData = tData.remove(0, 1);
        tData = tData.trimmed();
    }

    // Find any #'s past the start (these are comments)
    int idx = tData.indexOf('#');
    if (idx > 0) {
        // Save the comment, then remove from tData
        comment = tData.right(tData.size() - idx - 1);
        tData.remove(idx, tData.size() - idx + 1);
    }

    const QRegularExpression rx("^([a-z\\-]+) *");
    QRegularExpressionMatch match = rx.match(tData);

    // Parse type
    type = match.captured(1);
    const QSet<QString> types = { QLatin1String("rpm"), QLatin1String("rpm-src"), QLatin1String("deb"), QLatin1String("deb-src") };
    if (!match.isValid() || !types.contains(type)) {
        isValid = false;
        return;
    }
    
    int start = match.capturedEnd(), end = tData.size();
    // Parse architecture, see https://wiki.debian.org/Multiarch/HOWTO, Setting up sources
    if (tData[start] == '[') {
        QString metadata = tData.mid(start+1, tData.indexOf(']')-start-1);
        QStringList options = metadata.split(';');
        for (const QString &option : options) {
            QStringList parts = option.split('=');

            if (parts.size() != 2) {
                isValid = false;
                return;
            }

            QString key = parts.at(0);
            if (key != QLatin1String("arch")) {
                isValid = false;
                return;
            }

            QString value = parts.at(1);
            architectures = value.split(',');
        }
        
        start+=metadata.size()+2;
        for (; tData[start] == ' '; ++start)
        {}
    }
    
    bool inString = false;
    bool done = false;
    for (int i = start; !done && i<end; ++i) {
        switch (tData[i].toLatin1()) {
        case ' ':
            if (!inString) {
                uri = tData.mid(start, i-start);
                start = i+1;
                done = true;
            } break;
        case '[':
            inString = true;
            break;
        case ']':
            inString = false;
            break;
        }
    }
    if (uri.isEmpty() || !done) {
        isValid = false;
        return;
    }

    QStringList pieces = tData.mid(start).split(' ', QString::SkipEmptyParts);
    if (pieces.isEmpty()) {
        // Invalid source entry
        isValid = false;
        return;
    }

    // Parse distro and (optionally) components
    dist = pieces.takeFirst();
    components = pieces;
}

SourceEntry::SourceEntry()
    : d(new SourceEntryPrivate())
{
}

SourceEntry::SourceEntry(const QString &line, const QString &file)
    : d(new SourceEntryPrivate(line, file))
{
}

SourceEntry::SourceEntry(const QString &type, const QString &uri, const QString &dist,
                         const QStringList &comps, const QString &comment,
                         const QStringList &archs, const QString &file)
    : d(new SourceEntryPrivate(QString(), file))
{
    d->type = type;
    d->uri = uri;
    d->dist = dist;
    d->components = comps;
    d->comment = comment;
    d->architectures = archs;

    d->line = this->toString();
}

SourceEntry::SourceEntry(const SourceEntry &rhs)
    : d(rhs.d)
{
}

SourceEntry &SourceEntry::operator=(const SourceEntry &rhs)
{
    if (this != &rhs)
        d.operator=(rhs.d);
    return *this;
}

SourceEntry::~SourceEntry()
{
}

bool SourceEntry::operator==(const SourceEntry &other) const
{
    return (d->isEnabled == other.d->isEnabled &&
            d->type == other.d->type &&
            d->uri == other.d->uri &&
            d->dist == other.d->dist &&
            d->components == other.d->components);
}

bool SourceEntry::isValid() const
{
    return d->isValid;
}

bool SourceEntry::isEnabled() const
{
    return d->isEnabled;
}

QString SourceEntry::type() const
{
    return d->type;
}

QStringList SourceEntry::architectures() const
{
    return d->architectures;
}

QString SourceEntry::uri() const
{
    return d->uri;
}

QString SourceEntry::dist() const
{
    return d->dist;
}

QStringList SourceEntry::components() const
{
    return d->components;
}

QString SourceEntry::comment() const
{
    return d->comment;
}

QString SourceEntry::file() const
{
    return d->file;
}

QString SourceEntry::toString() const
{
    if (!d->isValid)
        return d->line.trimmed();

    QString line;

    if (!d->isEnabled)
        line = QLatin1String("# ");

    line += d->type;

    if (!d->architectures.isEmpty())
        line += QString(" [arch=%1]").arg(d->architectures.join(QChar(',')));

    line += ' ' % d->uri % ' ' % d->dist;

    if (!d->components.isEmpty())
        line += ' ' + d->components.join(QChar(' '));

    if (!d->comment.isEmpty())
        line += QLatin1String(" #") % d->comment;

    return line;
}

void SourceEntry::setEnabled(bool isEnabled)
{
    if (isEnabled == d->isEnabled)
        return;

    d->isEnabled = isEnabled;

    if (isEnabled) {
        // Remove #
        d->line.remove(0, 1);
    } else {
        // Add #
        d->line.prepend('#');
    }
}

void SourceEntry::setType(const QString &type)
{
    d->type = type;
}

void SourceEntry::setArchitectures(const QStringList &archs)
{
    d->architectures = archs;
}

void SourceEntry::setUri(const QString &uri)
{
    d->uri = uri;
}

void SourceEntry::setDist(const QString &dist)
{
    d->dist = dist;
}

void SourceEntry::setComponents(const QStringList &comps)
{
    d->components = comps;
}

void SourceEntry::setComment(const QString &comment)
{
    d->comment = comment;
}

void SourceEntry::setFile(const QString &file)
{
    d->file = file;
}

}
