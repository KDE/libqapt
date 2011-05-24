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

#include "DebThumbnailer.h"

#include <QtCore/QDir>
#include <QtCore/QStringBuilder>
#include <QtGui/QImage>
#include <QtGui/QPainter>

#include <KIcon>

#include "../../src/debfile.h"

extern "C"
{
    KDE_EXPORT ThumbCreator *new_creator()
    {
        return new DebThumbnailer;
    }
}


DebThumbnailer::DebThumbnailer()
{
}

DebThumbnailer::~DebThumbnailer()
{
}

bool DebThumbnailer::create(const QString &path, int width, int height, QImage &img)
{
    const QApt::DebFile debFile(path);

    if (!debFile.isValid()) {
        return false;
    }

    QStringList iconsList = debFile.iconList();
    qSort(iconsList);

    if (iconsList.isEmpty()) {
        return false;
    }

    QString iconPath = iconsList.last();

    QDir tempDir = QDir::temp();
    tempDir.mkdir(QLatin1String("kde-deb-thumbnailer"));

    QString destPath = QDir::tempPath() % QLatin1Literal("/kde-deb-thumbnailer/");

    if (!debFile.extractFileFromArchive(iconPath, destPath)) {
        return false;
    }

    QPixmap mimeIcon = KIcon("application-x-deb").pixmap(width, height);
    QPixmap appOverlay = QPixmap(destPath % iconPath).scaledToWidth(width/2);

    QPainter painter(&mimeIcon);
    for (int y = 0; y < appOverlay.height(); y += appOverlay.height()) {
        painter.drawPixmap( 0, y, appOverlay );
    }

    img = mimeIcon.toImage();

    return true;
}

ThumbCreator::Flags DebThumbnailer::flags() const
{
    return None;
}
