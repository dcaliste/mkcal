/*
  Copyright (c) 2022 Damien Caliste <dcaliste@free.fr>.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Library General Public
  License as published by the Free Software Foundation; either
  version 2 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Library General Public License for more details.

  You should have received a copy of the GNU Library General Public License
  along with this library; see the file COPYING.LIB.  If not, write to
  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
  Boston, MA 02110-1301, USA.
*/

#ifndef TST_ASYNC_H
#define TST_ASYNC_H

#include <QObject>
#include <QTemporaryFile>

#include "asyncsqlitestorage.h"

using namespace mKCal;

class QTemporaryFile;
class tst_async: public QObject
{
    Q_OBJECT

public:
    tst_async(QObject *parent = 0);

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    void tst_save();
    void tst_notebook();
    void tst_listing();
    void tst_load();
    void tst_batchLoad();
    void tst_directObserver();

private:
    ExtendedStorage::Ptr m_storage;
    QTemporaryFile *db;
};

#endif
