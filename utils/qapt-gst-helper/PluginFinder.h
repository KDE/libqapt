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

#ifndef PLUGINFINDER_H
#define PLUGINFINDER_H

#include <QObject>
#include <QList>

namespace QApt {
    class Backend;
    class Package;
}

class PluginInfo;

class PluginFinder : public QObject
{
    Q_OBJECT
public:
    explicit PluginFinder(QObject *parent = 0, QApt::Backend *backend = 0);
    ~PluginFinder();

private:
    QApt::Backend *m_backend;
    bool m_stop;
    QList<PluginInfo *> m_searchList;

public Q_SLOTS:
    void startSearch();
    void setSearchList(const QList<PluginInfo *> &list);
    void stop();

private Q_SLOTS:
    void find(const PluginInfo *pluginInfo);

Q_SIGNALS:
    void foundCodec(QApt::Package *package);
    void notFound();
};

#endif
