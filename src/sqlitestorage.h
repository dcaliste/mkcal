/*
  This file is part of the mkcal library.

  Copyright (c) 2009 Nokia Corporation and/or its subsidiary(-ies). All rights reserved.
  Copyright (c) 2014-2019 Jolla Ltd.
  Copyright (c) 2019 Open Mobile Platform LLC.

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
/**
  @file
  This file is part of the API for handling calendar data and
  defines the SqliteStorage class.

  @author Tero Aho \<ext-tero.1.aho@nokia.com\>
  @author Pertti Luukko \<ext-pertti.luukko@nokia.com\>
  @author Alvaro Manera \<alvaro.manera@nokia.com \>
*/

#ifndef MKCAL_SQLITESTORAGE_H
#define MKCAL_SQLITESTORAGE_H

#include "mkcal_export.h"
#include "extendedstorage.h"

namespace mKCal {

/**
  @brief
  This class provides a calendar storage as an sqlite database.

  @warning When saving Attendees, the CustomProperties are not saved.
*/
class MKCAL_EXPORT SqliteStorage : public ExtendedStorage
{
    Q_OBJECT

public:

    /**
      A shared pointer to a SqliteStorage
    */
    typedef QSharedPointer<SqliteStorage> Ptr;

    /**
      Constructs a new SqliteStorage object for Calendar @p calendar with
      storage to file @p databaseName.

      @param calendar is a pointer to a valid Calendar object.
      @param databaseName is the name of the database containing the Calendar data.
      @param validateNotebooks set to true for saving only those incidences
             that belong to an existing notebook of this storage
    */
    explicit SqliteStorage(const ExtendedCalendar::Ptr &cal,
                           const QString &databaseName,
                           bool validateNotebooks = true);

    /**
      Constructs a new SqliteStorage object for Calendar @p calendar. Location
      of the database is using default location, or is taken from SQLITESTORAGEDB
      enivronment variable.

      @param calendar is a pointer to a valid Calendar object.
      @param validateNotebooks set to true for saving only those incidences
             that belong to an existing notebook of this storage
    */
    explicit SqliteStorage(const ExtendedCalendar::Ptr &cal,
                           bool validateNotebooks = true);

    /**
      Destructor.
    */
    virtual ~SqliteStorage();

    /**
      Returns a string containing the name of the calendar database.
    */
    QString databaseName() const;

    /**
      @copydoc
      CalStorage::open()
    */
    bool open();

    /**
      @copydoc
      ExtendedStorage::loadContacts()
    */
    KCalendarCore::Person::List loadContacts();

    /**
      @copydoc
      ExtendedStorage::notifyOpened( const KCalendarCore::Incidence::Ptr & )
    */
    bool notifyOpened(const KCalendarCore::Incidence::Ptr &incidence);

    /**
      @copydoc
      ExtendedStorage::storeIncidences(const QMultiHash<QString, KCalendarCore::Incidence::Ptr> &,
                                       const QMultiHash<QString, KCalendarCore::Incidence::Ptr> &,
                                       const QMultiHash<QString, KCalendarCore::Incidence::Ptr> &,
                                       ExtendedStorage::DeleteAction)
    */
    bool storeIncidences(const QMultiHash<QString, KCalendarCore::Incidence::Ptr> &additions,
                         const QMultiHash<QString, KCalendarCore::Incidence::Ptr> &modifications,
                         const QMultiHash<QString, KCalendarCore::Incidence::Ptr> &deletions,
                         ExtendedStorage::DeleteAction deleteAction);

    /**
      @copydoc
      ExtendedStorage::purgeDeletedIncidences(const KCalCore::Incidence::List &)
    */
    bool purgeDeletedIncidences(const KCalendarCore::Incidence::List &list);

    /**
      @copydoc
      ExtendedStorage::cancel()
    */
    bool cancel();

    /**
      @copydoc
      CalStorage::close()
    */
    bool close();

    /**
      @copydoc
      ExtendedStorage::insertedIncidences()
    */
    bool insertedIncidences(KCalendarCore::Incidence::List *list, const QDateTime &after,
                            const QString &notebookUid = QString());

    /**
      @copydoc
      ExtendedStorage::modifiedIncidences()
    */
    bool modifiedIncidences(KCalendarCore::Incidence::List *list, const QDateTime &after,
                            const QString &notebookUid = QString());

    /**
      @copydoc
      ExtendedStorage::deletedIncidences()
    */
    bool deletedIncidences(KCalendarCore::Incidence::List *list,
                           const QDateTime &after = QDateTime(),
                           const QString &notebookUid = QString());

    /**
      @copydoc
      ExtendedStorage::allIncidences()
    */
    bool allIncidences(KCalendarCore::Incidence::List *list, const QString &notebookUid = QString());

    /**
      @copydoc
      ExtendedStorage::duplicateIncidences()
    */
    bool duplicateIncidences(KCalendarCore::Incidence::List *list,
                             const KCalendarCore::Incidence::Ptr &incidence,
                             const QString &notebookUid = QString());

    /**
      @copydoc
      ExtendedStorage::incidenceDeletedDate()
    */
    QDateTime incidenceDeletedDate(const KCalendarCore::Incidence::Ptr &incidence);

    /**
      @copydoc
      ExtendedStorage::eventCount()
    */
    int eventCount();

    /**
      @copydoc
      ExtendedStorage::todoCount()
    */
    int todoCount();

    /**
      @copydoc
      ExtendedStorage::journalCount()
    */
    int journalCount();

    /**
      Returns the time zone used by the storage.
     */
    QTimeZone timeZone() const;

    /**
      @copydoc
      ExtendedStorage::virtual_hook()
    */
    virtual void virtual_hook(int id, void *data);

protected:
    bool loadNotebooks(QList<Notebook> *notebooks, QString *defaultNotebookId);
    bool modifyNotebook(const Notebook &nb, DBOperation dbop);
    int loadIncidences(const ExtendedStorage::Filter &filter = ExtendedStorage::Filter()) override;
    int loadSortedIncidences(const ExtendedStorage::SortedFilter &filter = ExtendedStorage::SortedFilter(), int limit = -1, QDateTime *last = nullptr) override;

private:
    //@cond PRIVATE
    Q_DISABLE_COPY(SqliteStorage)
    class MKCAL_HIDE Private;
    Private *const d;
    //@endcond

public Q_SLOTS:
    void fileChanged(const QString &path);
};

}

#endif
