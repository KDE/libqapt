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
#include <QtGui/QPushButton>
#include <QtGui/QVBoxLayout>

#include <KGlobal>
#include <KIcon>
#include <KLocale>
#include <KTabWidget>
#include <KTextBrowser>
#include <KVBox>
#include <KDebug>

#include "../../src/backend.h"
#include "../../src/debfile.h"

#include "ChangesDialog.h"

DebViewer::DebViewer(QWidget *parent)
    : QWidget(parent)
    , m_backend(nullptr)
    , m_debFile(nullptr)
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
    namePrefixLabel->setText(i18nc("@label Label preceding the package name",
                                   "Package:"));
    m_nameLabel = new QLabel(nameStatusBox);
    nameStatusGrid->addWidget(namePrefixLabel, 0, 0, Qt::AlignRight);
    nameStatusGrid->addWidget(m_nameLabel, 0, 1, Qt::AlignLeft);

    // nameStatusGrid, row 1
    QLabel *statusPrefixLabel = new QLabel(nameStatusBox);
    statusPrefixLabel->setText(i18nc("@label Label preceding the package status",
                                     "Status:"));
    m_statusLabel = new QLabel(nameStatusBox);
    m_detailsButton = new QPushButton(i18nc("@label", "Details..."), nameStatusBox);
    m_detailsButton->hide();
    connect(m_detailsButton, SIGNAL(clicked()), this, SLOT(detailsButtonClicked()));
    nameStatusGrid->addWidget(statusPrefixLabel, 1, 0, Qt::AlignRight);
    nameStatusGrid->addWidget(m_statusLabel, 1, 1, Qt::AlignLeft);
    nameStatusGrid->addWidget(m_detailsButton, 1, 2, Qt::AlignLeft);

    headerLayout->addWidget(m_iconLabel);
    headerLayout->addWidget(nameStatusBox);
    headerLayout->addWidget(headerSpacer);


    // Version info box
    m_versionInfoWidget = new QWidget(this);
    QHBoxLayout *versionInfoLayout = new QHBoxLayout(m_versionInfoWidget);
    m_versionInfoWidget->setLayout(versionInfoLayout);

    QLabel *infoIcon = new QLabel(m_versionInfoWidget);
    infoIcon->setPixmap(KIcon("dialog-information").pixmap(32, 32));

    KVBox *verInfoBox = new KVBox(m_versionInfoWidget);
    m_versionTitleLabel = new QLabel(verInfoBox);
    m_versionInfoLabel = new QLabel(verInfoBox);

    QWidget *versionSpacer = new QWidget(m_versionInfoWidget);
    versionSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);

    versionInfoLayout->addWidget(infoIcon);
    versionInfoLayout->addWidget(verInfoBox);
    versionInfoLayout->addWidget(versionSpacer);
    //


    // Details tab widget
    KTabWidget *detailsWidget = new KTabWidget(this);

    KVBox *descriptionTab = new KVBox(detailsWidget);
    m_descriptionWidget = new KTextBrowser(descriptionTab);

    QWidget *detailsTab = new QWidget(detailsWidget);
    QGridLayout *detailsGrid = new QGridLayout(detailsTab);
    detailsTab->setLayout(detailsGrid);

    // detailsGrid, row 1
    QLabel *versionPrefixLabel = new QLabel(detailsTab);
    versionPrefixLabel->setText(i18nc("@label Label preceding the package version",
                                      "Version:"));
    m_versionLabel = new QLabel(detailsTab);
    detailsGrid->addWidget(versionPrefixLabel, 0, 0, Qt::AlignRight);
    detailsGrid->addWidget(m_versionLabel, 0, 1, Qt::AlignLeft);
    // detailsGrid, row 2
    QLabel *sizePrefixLabel = new QLabel(detailsTab);
    sizePrefixLabel->setText(i18nc("@label Label preceding the package size",
                                   "Installed Size:"));
    m_sizeLabel = new QLabel(detailsTab);
    detailsGrid->addWidget(sizePrefixLabel, 1, 0, Qt::AlignRight);
    detailsGrid->addWidget(m_sizeLabel, 1, 1, Qt::AlignLeft);
    // detailsGrid, row 3
    QLabel *maintainerPrefixLabel = new QLabel(detailsTab);
    maintainerPrefixLabel->setText(i18nc("@label Label preceding the package maintainer",
                                         "Maintainer:"));
    m_maintainerLabel = new QLabel(detailsTab);
    detailsGrid->addWidget(maintainerPrefixLabel, 2, 0, Qt::AlignRight);
    detailsGrid->addWidget(m_maintainerLabel, 2, 1, Qt::AlignLeft);
    // detailsGrid, row 4
    QLabel *sectionPrefixLabel = new QLabel(detailsTab);
    sectionPrefixLabel->setText(i18nc("@label Label preceding the package category",
                                      "Category:"));
    m_sectionLabel = new QLabel(detailsTab);
    detailsGrid->addWidget(sectionPrefixLabel, 3, 0, Qt::AlignRight);
    detailsGrid->addWidget(m_sectionLabel, 3, 1, Qt::AlignLeft);
    // detailsGrid, row 4
    QLabel *homepagePrefixLabel = new QLabel(detailsTab);
    homepagePrefixLabel->setText(i18nc("@label Label preceding the package homepage",
                                       "Homepage:"));
    m_homepageLabel = new QLabel(detailsTab);
    detailsGrid->addWidget(homepagePrefixLabel, 4, 0, Qt::AlignRight);
    detailsGrid->addWidget(m_homepageLabel, 4, 1, Qt::AlignLeft);

    detailsGrid->setColumnStretch(1, 1);
    detailsGrid->setRowStretch(5, 1);

    KVBox *fileTab = new KVBox(detailsWidget);
    m_fileWidget = new KTextBrowser(fileTab);


    detailsWidget->addTab(descriptionTab, i18nc("@title:tab", "Description"));
    detailsWidget->addTab(detailsTab, i18nc("@title:tab", "Details"));
    detailsWidget->addTab(fileTab, i18nc("@title:tab", "Included Files"));

    mainLayout->addWidget(headerWidget);
    mainLayout->addWidget(m_versionInfoWidget);
    mainLayout->addWidget(detailsWidget);
}

DebViewer::~DebViewer()
{
}

void DebViewer::setBackend(QApt::Backend *backend)
{
    m_backend = backend;
    m_oldCacheState = m_backend->currentCacheState();
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

    // Details tab widgets
    QString shortDesc = debFile->shortDescription();
    shortDesc.prepend(QLatin1String("<b>"));
    shortDesc.append(QLatin1String("</b><br><br>"));
    QString longDesc = debFile->longDescription();
    longDesc.replace('\n', QLatin1String("<br>"));

    m_descriptionWidget->append(shortDesc + longDesc);

    m_versionLabel->setText(debFile->version());
    m_sizeLabel->setText(KGlobal::locale()->formatByteSize(debFile->installedSize() * 1024));
    m_maintainerLabel->setText(debFile->maintainer());
    m_sectionLabel->setText(debFile->section());
    m_homepageLabel->setText(debFile->homepage());

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

void DebViewer::setStatusText(const QString &text)
{
    m_statusLabel->setText(text);
}

void DebViewer::showDetailsButton(bool show)
{
    m_detailsButton->setVisible(show);
}

void DebViewer::hideVersionInfo()
{
    m_versionInfoWidget->hide();
}

void DebViewer::setVersionTitle(const QString &title)
{
    m_versionTitleLabel->setText(title);
}

void DebViewer::setVersionInfo(const QString &info)
{
    m_versionInfoLabel->setText(info);
}

void DebViewer::detailsButtonClicked()
{
    QList<QApt::Package *> excluded;
    excluded.append(m_backend->package(m_debFile->packageName()));
    auto changes = m_backend->stateChanges(m_oldCacheState, excluded);

    if (changes.isEmpty())
        return;

    ChangesDialog *dialog = new ChangesDialog(this, changes);
    dialog->exec();
}
