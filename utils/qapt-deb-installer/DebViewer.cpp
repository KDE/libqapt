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

#include "DebViewer.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QStringBuilder>
#include <QtGui/QLabel>
#include <QtGui/QVBoxLayout>

#include <KIcon>
#include <KLocale>
#include <KTabWidget>
#include <KTextBrowser>
#include <KVBox>
#include <KDebug>

#include "../../src/backend.h"
#include "../../src/debfile.h"

DebViewer::DebViewer(QWidget *parent)
    : QWidget(parent)
    , m_debFile(0)
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    setLayout(mainLayout);

    // Header
    QWidget *headerWidget = new QWidget(this);
    QHBoxLayout *headerLayout = new QHBoxLayout(headerWidget);
    headerWidget->setLayout(headerLayout);

    m_iconLabel = new QLabel(headerWidget);

    QWidget *headerSpacer = new QWidget(headerWidget);
    headerSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);

    // Name/status box in header
    QWidget *nameStatusBox = new QWidget(headerWidget);
    QGridLayout *nameStatusGrid = new QGridLayout(nameStatusBox);
    nameStatusBox->setLayout(nameStatusGrid);

    // nameStatusGrid, row 0
    QLabel *namePrefixLabel = new QLabel(nameStatusBox);
    namePrefixLabel->setText(i18nc("@label Label preceeding the package name",
                                   "Package:"));
    m_nameLabel = new QLabel(nameStatusBox);
    nameStatusGrid->addWidget(namePrefixLabel, 0, 0, Qt::AlignRight);
    nameStatusGrid->addWidget(m_nameLabel, 0, 1, Qt::AlignLeft);

    // nameStatusGrid, row 1
    QLabel *statusPrefixLabel = new QLabel(nameStatusBox);
    statusPrefixLabel->setText(i18nc("@label Label preceeding the package status",
                                     "Status:"));
    m_statusLabel = new QLabel(nameStatusBox);
    nameStatusGrid->addWidget(statusPrefixLabel, 1, 0, Qt::AlignRight);
    nameStatusGrid->addWidget(m_statusLabel, 1, 1, Qt::AlignLeft);

    headerLayout->addWidget(m_iconLabel);
    headerLayout->addWidget(nameStatusBox);
    headerLayout->addWidget(headerSpacer);

    // Details tab widget
    KTabWidget *detailsWidget = new KTabWidget(this);

    KVBox *descriptionTab = new KVBox(detailsWidget);
    m_descriptionWidget = new KTextBrowser(descriptionTab);

    QWidget *detailsTab = new QWidget(detailsWidget);

    KVBox *fileTab = new KVBox(detailsWidget);
    m_fileWidget = new KTextBrowser(fileTab);

    detailsWidget->addTab(descriptionTab, i18nc("@title:tab", "Description"));
    detailsWidget->addTab(detailsTab, i18nc("@title:tab", "Details"));
    detailsWidget->addTab(fileTab, i18nc("@title:tab", "Included Files"));

    mainLayout->addWidget(headerWidget);
    mainLayout->addWidget(detailsWidget);
}

DebViewer::~DebViewer()
{
}

void DebViewer::setDebFile(QApt::DebFile *debFile)
{
    m_debFile = debFile;

    QStringList iconList = m_debFile->iconList();
    qSort(iconList);

    // Try to get the biggest icon, which should be last
    QString iconPath;
    if (!iconList.isEmpty()) {
        iconPath = iconList.last();
    }

    QDir tempDir = QDir::temp();
    tempDir.mkdir(QLatin1String("qapt-deb-installer"));

    QString destPath = QDir::tempPath() + QLatin1String("/qapt-deb-installer/");
    m_debFile->extractFileFromArchive(iconPath, destPath);

    QIcon icon;

    QString finalPath = destPath + iconPath;
    if (QFile::exists(destPath + iconPath)) {
        icon = QIcon(finalPath);
    }

    if (iconPath.isEmpty()) {
        icon = KIcon("application-x-deb");
    }

    m_iconLabel->setPixmap(icon.pixmap(48,48));

    m_nameLabel->setText(debFile->packageName());
    m_statusLabel->setText("A very packity package"); // FIXME: placeholder

    // Details tab widgets
    QString shortDesc = debFile->shortDescription();
    shortDesc.prepend(QLatin1String("<b>"));
    shortDesc.append(QLatin1String("</b><br><br>"));
    QString longDesc = debFile->longDescription();
    longDesc.replace('\n', QLatin1String("<br>"));

    m_descriptionWidget->append(shortDesc + longDesc);

    QStringList fileList = debFile->fileList();
    qSort(fileList);
    QString filesString;

    foreach (const QString &file, fileList) {
        if (!file.trimmed().isEmpty()) {
            filesString.append(file + '\n');
        }
    }

    m_fileWidget->setPlainText(filesString);
}

#include "DebViewer.moc"
