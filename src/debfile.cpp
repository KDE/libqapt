/***************************************************************************
 *   Copyright © 2011 Jonathan Thomas <echidnaman@kubuntu.org>             *
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

#include "debfile.h"

#include <QDir>
#include <QProcess>
#include <QStringBuilder>
#include <QTemporaryFile>

// Must be before APT_PKG_ABI checks!
#include <apt-pkg/macros.h>

#include <apt-pkg/debfile.h>
#include <apt-pkg/fileutl.h>
#if APT_PKG_ABI >= 600
#include <apt-pkg/hashes.h>
#else
#include <apt-pkg/md5.h>
#endif
#include <apt-pkg/tagfile.h>

#include <QDebug>

namespace QApt {

class DebFilePrivate
{
    public:
        DebFilePrivate(const QString &path)
            : isValid(false)
            , filePath(path)
            , extractor(0)
        {
            init();
        }

        ~DebFilePrivate()
        {
            delete extractor;
        }

        bool isValid;
        QString filePath;
        debDebFile::MemControlExtract *extractor;
        pkgTagSection *controlData;

        void init();
};

void DebFilePrivate::init()
{
    FileFd in(filePath.toUtf8().data(), FileFd::ReadOnly);
    debDebFile deb(in);

    // Extract control data
    try {
        extractor = new debDebFile::MemControlExtract("control");
        if(!extractor->Read(deb)) {
            return; // not valid.
        } else {
            isValid = true;
        }
    } catch (...) {
        // MemControlExtract likes to throw out of range exceptions when it
        // encounters an invalid file. Catch those to prevent the application
        // from exploding.
        return;
    }

    controlData = &extractor->Section;
}

DebFile::DebFile(const QString &filePath)
    : d(new DebFilePrivate(filePath))
{
}

DebFile::~DebFile()
{
    delete d;
}

bool DebFile::isValid() const
{
    return d->isValid;
}

QString DebFile::filePath() const
{
    return d->filePath;
}

QString DebFile::packageName() const
{
    return QString::fromStdString(d->controlData->FindS("Package"));
}

QString DebFile::sourcePackage() const
{
    return QString::fromStdString(d->controlData->FindS("Source"));
}

QString DebFile::version() const
{
    return QString::fromStdString(d->controlData->FindS("Version"));
}

QString DebFile::architecture() const
{
    return QString::fromStdString(d->controlData->FindS("Architecture"));
}

QString DebFile::maintainer() const
{
    return QString::fromStdString(d->controlData->FindS("Maintainer"));
}

QString DebFile::section() const
{
    return QString::fromStdString(d->controlData->FindS("Section"));
}

QString DebFile::priority() const
{
    return QString::fromStdString(d->controlData->FindS("Priority"));
}

QString DebFile::homepage() const
{
    return QString::fromStdString(d->controlData->FindS("Homepage"));
}

QString DebFile::longDescription() const
{
    QString rawDescription = QLatin1String(d->controlData->FindS("Description").c_str());
    // Remove short description
    rawDescription.remove(shortDescription() + '\n');

    QString parsedDescription;
    // Split at double newline, by "section"
    QStringList sections = rawDescription.split(QLatin1String("\n ."));

    for (int i = 0; i < sections.count(); ++i) {
        sections[i].replace(QRegExp(QLatin1String("\n( |\t)+(-|\\*)")),
                                QLatin1String("\n\r ") % QString::fromUtf8("\xE2\x80\xA2"));
        // There should be no new lines within a section.
        sections[i].remove(QLatin1Char('\n'));
        // Hack to get the lists working again.
        sections[i].replace(QLatin1Char('\r'), QLatin1Char('\n'));
        // Merge multiple whitespace chars into one
        sections[i].replace(QRegExp(QLatin1String("\\ \\ +")), QChar::fromLatin1(' '));
        // Remove the initial whitespace
        sections[i].remove(0, 1);
        // Append to parsedDescription
        if (sections[i].startsWith(QLatin1String("\n ") % QString::fromUtf8("\xE2\x80\xA2 ")) || !i) {
            parsedDescription += sections[i];
        }  else {
            parsedDescription += QLatin1String("\n\n") % sections[i];
        }
    }
    return parsedDescription;
}

QString DebFile::shortDescription() const
{
    QString longDesc = QLatin1String(d->controlData->FindS("Description").c_str());

    return longDesc.left(longDesc.indexOf(QLatin1Char('\n')));
}

QString DebFile::controlField(const QLatin1String &field) const
{
    return QString::fromStdString(d->controlData->FindS(field.latin1()));
}

QString DebFile::controlField(const QString &field) const
{
    return controlField(QLatin1String(field.toLatin1()));
}

QByteArray DebFile::md5Sum() const
{
    FileFd in(d->filePath.toStdString(), FileFd::ReadOnly);
    debDebFile deb(in);
#if APT_PKG_ABI >= 600
    Hashes debMD5(Hashes::MD5SUM);
#else
    MD5Summation debMD5;
#endif

    in.Seek(0);

    debMD5.AddFD(in.Fd(), in.Size());

#if APT_PKG_ABI >= 600
    return QByteArray::fromStdString(debMD5.GetHashString(Hashes::MD5SUM).HashValue());
#else
    return QByteArray::fromStdString(debMD5.Result().Value());
#endif
}

QStringList DebFile::fileList() const
{
    QTemporaryFile tempFile;
    if (!tempFile.open()) {
        return QStringList();
    }

    QString tempFileName = tempFile.fileName();
    QProcess dpkg;
    QProcess tar;

    // dpkg --fsys-tarfile filename
    QString program = QLatin1String("dpkg --fsys-tarfile ") + d->filePath;

    dpkg.setStandardOutputFile(tempFileName);
    dpkg.start(program);
    dpkg.waitForFinished();

    QString program2 = QLatin1String("tar -tf ") + tempFileName;
    tar.start(program2);
    tar.waitForFinished();

    QString files = tar.readAllStandardOutput();

    QStringList filesList = files.split('\n');
    filesList.removeFirst(); // First entry is the "./" entry
    filesList.removeAll(QLatin1String("")); // Remove empty entries

    // Remove non-file directory listings
    for (int i = 0; i < filesList.size() - 1; ++i) {
        if (filesList.at(i+1).contains(filesList.at(i))) {
            filesList[i] = QString(QLatin1Char(' '));
        }

        filesList.removeAll(QChar::fromLatin1(' '));
    }

    return filesList;
}

QStringList DebFile::iconList() const
{
    QStringList fileNames = fileList();
    QStringList iconsList;
    foreach (const QString &fileName, fileNames) {
        if (fileName.startsWith(QLatin1String("./usr/share/icons"))) {
            iconsList << fileName;
        }
    }

    // XPM as a fallback. It's really not pretty when scaled up
    if (iconsList.isEmpty()) {
        foreach (const QString &fileName, fileNames) {
            if (fileName.startsWith(QLatin1String("./usr/share/pixmaps"))) {
                iconsList << fileName;
            }
        }
    }

    return iconsList;
}

QList<DependencyItem> DebFile::depends() const
{
    return DependencyInfo::parseDepends(QString::fromStdString(d->controlData->FindS("Depends")), Depends);
}

QList<DependencyItem> DebFile::preDepends() const
{
    return DependencyInfo::parseDepends(QString::fromStdString(d->controlData->FindS("Pre-Depends")), PreDepends);
}

QList<DependencyItem> DebFile::suggests() const
{
    return DependencyInfo::parseDepends(QString::fromStdString(d->controlData->FindS("Suggests")), Suggests);
}

QList<DependencyItem> DebFile::recommends() const
{
    return DependencyInfo::parseDepends(QString::fromStdString(d->controlData->FindS("Recommends")), Recommends);
}

QList<DependencyItem> DebFile::conflicts() const
{
    return DependencyInfo::parseDepends(QString::fromStdString(d->controlData->FindS("Conflicts")), Conflicts);
}

QList<DependencyItem> DebFile::replaces() const
{
    return DependencyInfo::parseDepends(QString::fromStdString(d->controlData->FindS("Replaces")), Replaces);
}

QList<DependencyItem> DebFile::obsoletes() const
{
    return DependencyInfo::parseDepends(QString::fromStdString(d->controlData->FindS("Obsoletes")), Obsoletes);
}

QList<DependencyItem> DebFile::breaks() const
{
    return DependencyInfo::parseDepends(QString::fromStdString(d->controlData->FindS("Breaks")), Breaks);
}

QList<DependencyItem> DebFile::enhances() const
{
    return DependencyInfo::parseDepends(QString::fromStdString(d->controlData->FindS("Enhance")), Enhances);
}

qint64 DebFile::installedSize() const
{
    QString sizeString = QLatin1String(d->controlData->FindS("Installed-Size").c_str());

    return sizeString.toLongLong();
}

bool DebFile::extractArchive(const QString &basePath) const
{
    // The deb extractor extracts to the working path.
    QString oldCurrent = QDir::currentPath();

    if (!basePath.isEmpty()) {
        QDir::setCurrent(basePath);
    }

    FileFd in(d->filePath.toStdString(), FileFd::ReadOnly);
    debDebFile deb(in);

    pkgDirStream stream;
    bool res = deb.ExtractArchive(stream);

    // Restore working path once we are done
    if (!basePath.isEmpty()) {
        QDir::setCurrent(oldCurrent);
    }

    return res;
}

bool DebFile::extractFileFromArchive(const QString &fileName, const QString &destination) const
{
    QTemporaryFile tempFile;
    if (!tempFile.open()) {
        return false;
    }

    QString tempFileName = tempFile.fileName();

    // dpkg --fsys-tarfile filename
    QString program = QLatin1String("dpkg --fsys-tarfile ") + d->filePath;

    QProcess dpkg;
    dpkg.setStandardOutputFile(tempFileName);
    dpkg.start(program);
    dpkg.waitForFinished();

    QString program2 = QLatin1String("tar -xf") % tempFileName %
                       QLatin1String(" -C ") % destination % ' ' % fileName;

    QProcess tar;
    tar.start(program2);
    tar.waitForFinished();

    return !tar.exitCode();
}

}
