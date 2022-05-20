/*
  This file is part of the mkcal library.

  Copyright (c) 2009 Nokia Corporation and/or its subsidiary(-ies). All rights reserved.
  Contact: Alvaro Manera <alvaro.manera@nokia.com>

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
#ifndef DUMMYSTORAGE_H
#define DUMMYSTORAGE_H

#include "extendedstorage.h"
#include "extendedcalendar.h"
#include "notebook.h"


/**
 * This module provides a simple storage abstraction which contains
 * exactly nothing. It is only inteded to use for testing purposes
 */

class MKCAL_EXPORT DummyStorage : public mKCal::ExtendedStorage
{
public:
    DummyStorage(const mKCal::ExtendedCalendar::Ptr &cal) : mKCal::ExtendedStorage(cal)
    {
        mKCal::Notebook nb("dummy-name", "dummy-desc");
        bool r;
        r = addNotebook(nb);
        Q_ASSERT(r);
        r = setDefaultNotebook(nb);
        Q_ASSERT(r);
    }

    bool purgeDeletedIncidences(const KCalendarCore::Incidence::List &)
    {
        return true;
    }

    /**
      @copydoc
      ExtendedStorage::open()
    */
    bool open()
    {
        return true;
    }

    KCalendarCore::Person::List loadContacts()
    {
        KCalendarCore::Person::List l;
        return l;
    }
    bool notifyOpened(const KCalendarCore::Incidence::Ptr &)
    {
        return true;
    }
    bool cancel()
    {
        return true;
    }
    bool loadNotebooks(QList<Notebook> *notebooks, QString *defaultNotebookId)
    {
        return true;
    }
    bool modifyNotebook(const mKCal::Notebook &, mKCal::DBOperation)
    {
        return true;
    }
    bool storeIncidences(const QMultiHash<QString, KCalendarCore::Incidence::Ptr> &additions,
                         const QMultiHash<QString, KCalendarCore::Incidence::Ptr> &modification,
                         const QMultiHash<QString, KCalendarCore::Incidence::Ptr> &deletions,
                         ExtendedStorage::DeleteAction deleteAction)
    {
        return true;
    }
    bool loadIncidences(QMultiHash<QString, KCalendarCore::Incidence::Ptr> *incidences, const ExtendedStorage::Filter &filter = ExtendedStorage::Filter()) const
    {
        return true;
    }
    bool loadSortedIncidences(QMultiHash<QString, KCalendarCore::Incidence::Ptr> *incidences, const ExtendedStorage::SortedFilter &filter = ExtendedStorage::SortedFilter(),
                             int limit = -1, QDateTime *last = nullptr) const
    {
        return true;
    }
    QDateTime incidenceDeletedDate(const KCalendarCore::Incidence::Ptr &)
    {
        return QDateTime();
    }
    int eventCount()
    {
        return 0;
    }
    int todoCount()
    {
        return 0;
    }
    int journalCount()
    {
        return 0;
    }
    QTimeZone timeZone() const
    {
        return QTimeZone();
    }
    void virtual_hook(int, void *)
    {
        return;
    }
};

#endif /* DUMMYSTORAGE_H */
