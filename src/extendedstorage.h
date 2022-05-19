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
/**
  @file
  This file is part of the API for handling calendar data and
  defines the ExtendedStorage interface.

  @author Tero Aho \<ext-tero.1.aho@nokia.com\>
  @author Alvaro Manera \<alvaro.manera@nokia.com \>
*/

#ifndef MKCAL_EXTENDEDSTORAGE_H
#define MKCAL_EXTENDEDSTORAGE_H

#include "mkcal_export.h"
#include "extendedcalendar.h"
#include "extendedstorageobserver.h"
#include "notebook.h"

#include <KCalendarCore/CalStorage>
#include <KCalendarCore/Calendar>

namespace KCalendarCore {
class Incidence;
}

class MkcalTool;
class tst_load;

namespace mKCal {

/**
  Database operation type.
*/
enum DBOperation {
    DBNone,
    DBInsert,
    DBUpdate,
    DBMarkDeleted,
    DBDelete,
    DBSelect
};

/**
  @brief
  This class provides a calendar storage interface.
  Every action on the storage can be asynchronous, which means that actions
  are only scheduled for execution. Caller must use ExtendedStorageObserver to get
  notified about the completion.
*/
class MKCAL_EXPORT ExtendedStorage
    : public KCalendarCore::CalStorage
{
    Q_OBJECT

public:

    /**
      Action to be performed on save for deleted incidences.
    */
    enum DeleteAction {
        MarkDeleted,
        PurgeDeleted
    };

    /**
      A shared pointer to a ExtendedStorage
    */
    typedef QSharedPointer<ExtendedStorage> Ptr;

    /**
      Constructs a new ExtendedStorage object.

      @param cal is a pointer to a valid Calendar object.
      @param validateNotebooks set to true for loading/saving only those
             incidences that belong to an existing notebook of this storage

      @warning Do not use storage as a global object, on closing the application
      it can dead lock. If you do so, be ready to destroy it manually before the
      application closes.

      @warning Once an Incidence has been added to the ExtendedStorage the UID
      cannot change. It is possible to do so through the API, but the internal
      hash tables will not be updated and hence the changes will not be tracked.
    */
    explicit ExtendedStorage(const ExtendedCalendar::Ptr &cal, bool validateNotebooks = true);

    /**
      Destructor.
    */
    virtual ~ExtendedStorage();

    /**
      @copydoc
      CalStorage::open()
    */
    virtual bool open();

    /**
      @copydoc
      CalStorage::load()
    */
    virtual bool load();

    /**
      Load incidence by uid into the memory.

      @param uid is uid of incidence
      @param recurrenceid is recurrenceid of incidence, default null
      @return true if the load was successful; false otherwise.
    */
    virtual bool load(const QString &uid, const QDateTime &recurrenceId = QDateTime());

    /**
      Load incidences at given date into the memory. All incidences that
      happens within date, or starts / ends within date or span
      during date are loaded into memory. The time zone used to expand
      date into points in time is the time zone of the associated calendar.
      In addition, all recurring events are also loaded into memory since
      there is no way to know in advance if they will have occurrences
      intersecting date. Internally, recurring incidences and incidences of
      date are cached to avoid loading them several times.

      @param date date
      @return true if the load was successful; false otherwise.
    */
    virtual bool load(const QDate &date);

    /**
      Load incidences between given dates into the memory. start is inclusive,
      while end is exclusive. The same definitions and restrictions for loading
      apply as for load(const QDate &) method.

      @param start is the starting date
      @param end is the ending date, exclusive
      @return true if the load was successful; false otherwise.
    */
    virtual bool load(const QDate &start, const QDate &end);

    /**
      Load all incidences sharing the same uid into the memory.

      @param uid is uid of the series
      @return true if the load was successful; false otherwise.
    */
    virtual bool loadSeries(const QString &uid);

    /**
      Load the incidence matching the given identifier. This method may be
      more fragile than load(uid, recid) though since the instanceIdentifier
      is not saved as is in the database.

      @param instanceIdentifier is an identifier returned by Incidence::instanceIdentifier()
      @return true if the load was successful; false otherwise.
    */
    virtual bool loadIncidenceInstance(const QString &instanceIdentifier);

    /**
      Load incidences of one notebook into the memory.

      @param notebookUid is uid of notebook
      @return true if the load was successful; false otherwise.
    */
    virtual bool loadNotebookIncidences(const QString &notebookUid);

    /**
      Load journal type entries
    */
    virtual bool loadJournals();

    /**
      Load plain incidences (no startdate and no enddate).

      @return true if the load was successful; false otherwise.
    */
    virtual bool loadPlainIncidences();

    /**
      Load recurring incidences.

      @return true if the load was successful; false otherwise.
    */
    virtual bool loadRecurringIncidences();

    /**
      Load incidences that have geo parameters.

      @return true if the load was successful; false otherwise.
    */
    virtual bool loadGeoIncidences();

    /**
      Load incidences that have geo parameters inside given rectangle.

      @param geoLatitude latitude
      @param geoLongitude longitude
      @param diffLatitude maximum latitudinal difference
      @param diffLongitude maximum longitudinal difference
      @return true if the load was successful; false otherwise.
    */
    virtual bool loadGeoIncidences(float geoLatitude, float geoLongitude,
                                   float diffLatitude, float diffLongitude);

    /**
      Load incidences that have attendee.

      @return true if the load was successful; false otherwise.
    */
    virtual bool loadAttendeeIncidences();

    // Smart Loading Functions //

    /**
      Load all uncompleted todos.

      @return number of loaded todos, or -1 on error
    */
    virtual int loadUncompletedTodos();

    /**
      Load completed todos based on parameters. Load direction is descending,
      i.e., starting from most distant upcoming todo.

      @param hasDate set true to load todos that have due date
      @param limit load only that many todos
      @param last last loaded todo due/creation date in return
      @return number of loaded todos, or -1 on error
    */
    virtual int loadCompletedTodos(bool hasDate, int limit, QDateTime *last);

    /**
      Load incidences based on start/due date or creation date.
      Load direction is descending, i.e., starting from most distant
      upcoming incidence.

      @param hasDate set true to load incidences that have start/due date
      @param limit load only that many incidences
      @param last last loaded incidence due/creation date in return
      @return number of loaded incidences, or -1 on error
    */
    virtual int loadIncidences(bool hasDate, int limit, QDateTime *last);

    /**
      Load future incidences based on start/due date.

      Load direction is ascending, i.e., starting from the oldest
      event that is still valid at the day of the loadIncidences
      call. (=end time > 00:00:00 on that day).

      @param limit load only that many incidences
      @param last last loaded incidence start date in return
      @return number of loaded incidences, or -1 on error
    */
    virtual int loadFutureIncidences(int limit, QDateTime *last);

    /**
      Load incidences that have location information based on parameters.
      Load direction is descending, i.e., starting from most distant
      upcoming incidence.

      @param hasDate set true to load incidences that have start/due date
      @param limit load only that many incidences
      @param last last loaded incidence due/creation date in return
      @return number of loaded incidences, or -1 on error
    */
    virtual int loadGeoIncidences(bool hasDate, int limit, QDateTime *last);

    /**
      Load all contacts in the database. Doesn't put anything into calendar.
      Resulting list of persons is ordered by the number of appearances.
      Use Person::count to get the number of appearances.

      @return ordered list of persons
    */
    virtual KCalendarCore::Person::List loadContacts() = 0;

    /**
      Load all incidences that have the specified attendee.
      Also includes all shared notes (in a shared notebook).

      @param person person in question
      @param limit load only that many incidences
      @param last last loaded incidence due/creation date in return
      @return number of loaded incidences, or -1 on error
    */
    virtual int loadContactIncidences(const KCalendarCore::Person &person,
                                      int limit, QDateTime *last);

    /**
      Load journal entries based on parameters. Load direction is
      descending, i.e. starting from the most recently modified
      journal.

      @param limit load only that many incidences
      @param last last loaded incidence due/creation date in return
      @return number of loaded incidences, or -1 on error
    */
    virtual int loadJournals(int limit, QDateTime *last);

    /**
      Remove from storage all incidences that have been previously
      marked as deleted and that matches the UID / RecID of the incidences
      in list. The action is performed immediately on database.

      @return True on success, false otherwise.
     */
    virtual bool purgeDeletedIncidences(const KCalendarCore::Incidence::List &list) = 0;

    /**
      @copydoc
      CalStorage::save()
    */
    virtual bool save();

    /**
      This is an overload of save() method. When @deleteAction is
      PurgeDeleted, the deleted incidences are not marked as deleted but completely
      removed from the database and won't appear anymore when calling
      deletedIncidences().

      @param deleteAction the action to apply to deleted incidences
      @return True if successful; false otherwise
    */
    virtual bool save(DeleteAction deleteAction);

    /**
      Mark if supported by the storage that an incidence has been opened.
      This should be called only if the Incidence has been opened by the user
      and displayed all the contents. Being in a list doesn't qualify for it.

      @param incidence The incidence that has been opened
      @return True if sucessful; false otherwise
    */
    virtual bool notifyOpened(const KCalendarCore::Incidence::Ptr &incidence) = 0;

    /**
      Cancel any ongoing action (load etc.).

      @return true cancel was successful; false otherwise
    */
    virtual bool cancel() = 0;

    /**
      @copydoc
      CalStorage::close()
    */
    virtual bool close();

    // Synchronization Specific Methods //

    /**
      Get inserted incidences from storage.

      NOTE: time stamps assigned by KCalExtended are created during save().
      To obtain a time stamp that is guaranteed to not included recent changes,
      sleep for a second or increment the current time by a second.

      @param list inserted incidences
      @param after list only incidences inserted after or at given datetime
      @param notebookUid list only incidences for given notebook
      @return true if execution was scheduled; false otherwise
    */
    virtual bool insertedIncidences(KCalendarCore::Incidence::List *list,
                                    const QDateTime &after = QDateTime(),
                                    const QString &notebookUid = QString()) = 0;

    /**
      Get modified incidences from storage.
      NOTE: if an incidence is both created and modified after the
      given time, it will be returned in insertedIncidences only, not here!

      @param list modified incidences
      @param after list only incidences modified after or at given datetime
      @param notebookUid list only incidences for given notebook
      @return true if execution was scheduled; false otherwise
    */
    virtual bool modifiedIncidences(KCalendarCore::Incidence::List *list,
                                    const QDateTime &after = QDateTime(),
                                    const QString &notebookUid = QString()) = 0;

    /**
      Get deleted incidences from storage.

      @param list deleted incidences
      @param after list only incidences deleted after or at given datetime
      @param notebookUid list only incidences for given notebook
      @return true if execution was scheduled; false otherwise
    */
    virtual bool deletedIncidences(KCalendarCore::Incidence::List *list,
                                   const QDateTime &after = QDateTime(),
                                   const QString &notebookUid = QString()) = 0;

    /**
      Get all incidences from storage.

      @param list notebook's incidences
      @param notebookUid list incidences for given notebook
      @return true if execution was scheduled; false otherwise
    */
    virtual bool allIncidences(KCalendarCore::Incidence::List *list,
                               const QString &notebookUid = QString()) = 0;

    /**
      Get possible duplicates for given incidence.

      @param list duplicate incidences
      @param incidence incidence to check
      @param notebookUid list incidences for given notebook
      @return true if execution was scheduled; false otherwise
    */
    virtual bool duplicateIncidences(KCalendarCore::Incidence::List *list,
                                     const KCalendarCore::Incidence::Ptr &incidence,
                                     const QString &notebookUid = QString()) = 0;

    /**
      Get deletion time of incidence

      @param incidence incidence to check
      @return valid deletion time of incidence in UTC if incidence has been deleted otherwise QDateTime()
    */
    virtual QDateTime incidenceDeletedDate(const KCalendarCore::Incidence::Ptr &incidence) = 0;

    /**
      Get count of events

      @return count of events
    */
    virtual int eventCount() = 0;

    /**
      Get count of todos

      @return count of todos
    */
    virtual int todoCount() = 0;

    /**
      Get count of journals

      @return count of journals
    */
    virtual int journalCount() = 0;

    // Observer Specific Methods //

    /**
      Registers an Observer for this Storage.

      @param observer is a pointer to an Observer object that will be
      watching this Storage.

      @see unregisterObserver()
     */
    void registerObserver(ExtendedStorageObserver *observer);

    /**
      Unregisters an Observer for this Storage.

      @param observer is a pointer to an Observer object that has been
      watching this Storage.

      @see registerObserver()
     */
    void unregisterObserver(ExtendedStorageObserver *observer);

    // Notebook Methods //

    /**
      Add new notebook to the storage.
      Operation is executed immediately into storage.

      @param nb notebook
      @return true if operation was successful; false otherwise.
    */
    bool addNotebook(const Notebook &nb);

    /**
      Update notebook parameters.
      Operation is executed immediately into storage.

      @param nb notebook
      @return true if add was successful; false otherwise.
    */
    bool updateNotebook(const Notebook &nb);

    /**
      Delete notebook from storage.
      Operation is executed immediately into storage.

      @param nbid the id of the notebook to delete
      @return true if delete was successful; false otherwise.
    */
    bool deleteNotebook(const QString &nbid);

    /**
      setDefaultNotebook to the storage.

      @param nb notebook
      @return true if operation was successful; false otherwise.
    */
    bool setDefaultNotebook(const Notebook &nb);

    /**
      defaultNotebookId.

      @return the default notebook id, if any.
    */
    QString defaultNotebookId() const;

    /**
      Search for notebook.

      @param uid notebook uid
      @return the notebook
    */
    Notebook notebook(const QString &uid) const;

    /**
      List all notebooks.

      @return list of notebooks
    */
    QList<Notebook> notebooks() const;

    /**
      containsNotebook.

      @return true if the storage contains the notebook identified by uid.
    */
    bool containsNotebook(const QString &uid) const;

    /**
      Determine if notebooks should be validated in saves and loads.
      That means that storage can only load/save incidences into/from
      existing notebooks.

      @param validate true to validate
    */
    void setValidateNotebooks(bool validateNotebooks);

    /**
      Returns true if notebooks should be validated in saves and loads.
      That means that storage can only load/save incidences into/from
      existing notebooks.

      @return true to validate notebooks
    */
    bool validateNotebooks() const;

    // Alarm Methods //

    /**
      Checking if an incidence has active alarms.
      Application can use this function for getting the incidence in
      question, for example, displaying the incidence after an alarm.

      @param uid uid
      @param recurrenceId recurrenceId
      @param loadAlways set true to load always from storage
      @return the alarmed incidence, or null if there is no active alarm
    */
    KCalendarCore::Incidence::Ptr checkAlarm(const QString &uid, const QString &recurrenceId,
                                        bool loadAlways = false);

    /**
      Standard trick to add virtuals later.

      @param id is any integer unique to this class which we will use to identify the method
             to be called.
      @param data is a pointer to some glob of data, typically a struct.
    */
    virtual void virtual_hook(int id, void *data) = 0;

protected:
    class Filter
    {
    public:
        enum FilterType {
            None,
            ByNotebook,
            ByIncidence,
            BySeries,
            ByDatetimeRange,
            ByNoDate,
            SortedByDatetime,
            ByJournal,
            ByTodo,
            Recursive,
            ByGeoLocation,
            ByAttendee
        };
        Filter() {};
        virtual FilterType type() const { return None; };
    };

    class NotebookFilter: public Filter
    {
    public:
        NotebookFilter(const QString &notebookUid) : mNotebookUid(notebookUid) {};
        FilterType type() const { return ByNotebook; };
        QString notebookUid() const { return mNotebookUid; };
    private:
        QString mNotebookUid;
    };

    class IncidenceFilter: public Filter
    {
    public:
        IncidenceFilter(const QString &uid, const QDateTime &recurrenceId = QDateTime())
            : mUid(uid), mRecurrenceId(recurrenceId) {};
        FilterType type() const { return ByIncidence; };
        QString uid() const { return mUid; };
        QDateTime recurrenceId() const { return mRecurrenceId; };
    private:
        QString mUid;
        QDateTime mRecurrenceId;
    };

    class SeriesFilter: public Filter
    {
    public:
        SeriesFilter(const QString &uid) : mUid(uid) {};
        FilterType type() const { return BySeries; };
        QString uid() const { return mUid; };
    private:
        QString mUid;
    };

    class RangeFilter: public Filter
    {
    public:
        RangeFilter(const QDateTime &start, const QDateTime &end)
            : mStart(start), mEnd(end) {};
        FilterType type() const { return ByDatetimeRange; };
        QDateTime start() const { return mStart; };
        QDateTime end() const { return mEnd; };
    private:
        QDateTime mStart, mEnd;
    };

    class NoDateFilter: public Filter
    {
    public:
        NoDateFilter() {};
        FilterType type() const { return ByNoDate; };
    };

    class SortedFilter: public Filter
    {
    public:
        SortedFilter(bool useDate = false, bool before = true)
            : mUseDate(useDate), mBefore(before) {};
        FilterType type() const { return SortedByDatetime; };
        bool useDate() const { return mUseDate; };
        bool before() const { return mBefore; };
    private:
        bool mUseDate, mBefore;
    };

    class JournalFilter: public SortedFilter
    {
    public:
        JournalFilter() {};
        FilterType type() const { return ByJournal; };
    };

    class TodoFilter: public SortedFilter
    {
    public:
        TodoFilter() : mCompleted(false) {};
        TodoFilter(bool useDate)
            : SortedFilter(useDate)
            , mCompleted(true) {};
        FilterType type() const { return ByTodo; };
        bool completed() const { return mCompleted; };
    private:
        bool mCompleted;
    };

    class RecursiveFilter: public Filter
    {
    public:
        RecursiveFilter() {};
        FilterType type() const { return Recursive; };
    };

    class GeoLocationFilter: public SortedFilter
    {
    public:
        GeoLocationFilter()
            : mLatitude(0.f), mLongitude(0.f)
            , mDeltaLatitude(180.f), mDeltaLongitude(360.f) {};
        GeoLocationFilter(float latitude, float longitude,
                          float deltaLatitude, float deltaLongitude)
            : mLatitude(latitude), mLongitude(longitude)
            , mDeltaLatitude(deltaLatitude), mDeltaLongitude(deltaLongitude) {};
        GeoLocationFilter(bool useDate) : SortedFilter(useDate) {};
        FilterType type() const { return ByGeoLocation; };
        float latitude() const { return mLatitude; };
        float longitude() const { return mLongitude; };
        float deltaLatitude() const { return mDeltaLatitude; };
        float deltaLongitude() const { return mDeltaLongitude; };
    private:
        float mLatitude, mLongitude, mDeltaLatitude, mDeltaLongitude;
    };

    class AttendeeFilter: public SortedFilter
    {
    public:
        AttendeeFilter() {};
        AttendeeFilter(const QString &email) : mEmail(email) {};
        FilterType type() const { return ByAttendee; };
        QString email() const { return mEmail; };
    private:
        QString mEmail;
    };

    virtual QTimeZone timeZone() const = 0;
    virtual bool loadNotebooks(QList<Notebook> *notebooks, QString *defaultNotebookId) = 0;
    virtual bool modifyNotebook(const Notebook &nb, DBOperation dbop) = 0;
    virtual bool storeIncidences(const QMultiHash<QString, KCalendarCore::Incidence::Ptr> &additions,
                                 const QMultiHash<QString, KCalendarCore::Incidence::Ptr> &modifications,
                                 const QMultiHash<QString, KCalendarCore::Incidence::Ptr> &deletions,
                                 ExtendedStorage::DeleteAction deleteAction) = 0;
    virtual int loadIncidences(const Filter &filter = Filter()) = 0;
    virtual int loadSortedIncidences(const SortedFilter &filter = SortedFilter(),
                                     int limit = -1, QDateTime *last = nullptr) = 0;

    void setModified(const QString &info);
    void setFinished(bool error, const QString &info);
    void setUpdated(const KCalendarCore::Incidence::List &added,
                    const KCalendarCore::Incidence::List &modified,
                    const KCalendarCore::Incidence::List &deleted);
    void setLoaded(const QMultiHash<QString, KCalendarCore::Incidence::Ptr> &incidences);
private:
    bool getLoadDates(const QDate &start, const QDate &end,
                      QDateTime *loadStart, QDateTime *loadEnd) const;

    void addLoadedRange(const QDate &start, const QDate &end) const;
    bool isRecurrenceLoaded() const;
    void setIsRecurrenceLoaded(bool loaded);

    bool isUncompletedTodosLoaded();
    void setIsUncompletedTodosLoaded(bool loaded);

    bool isCompletedTodosDateLoaded();
    void setIsCompletedTodosDateLoaded(bool loaded);
    bool isCompletedTodosCreatedLoaded();
    void setIsCompletedTodosCreatedLoaded(bool loaded);

    bool isJournalsLoaded();
    void setIsJournalsLoaded(bool loaded);

    bool isDateLoaded();
    void setIsDateLoaded(bool loaded);
    bool isCreatedLoaded();
    void setIsCreatedLoaded(bool loaded);
    bool isFutureDateLoaded();
    void setIsFutureDateLoaded(bool loaded);

    bool isGeoDateLoaded();
    void setIsGeoDateLoaded(bool loaded);
    bool isGeoCreatedLoaded();
    void setIsGeoCreatedLoaded(bool loaded);

    void clearLoaded();

    //@cond PRIVATE
    Q_DISABLE_COPY(ExtendedStorage)
    class MKCAL_HIDE Private;
    Private *const d;
    //@endcond

    friend class ::MkcalTool;
    friend class ::tst_load;
};

}

#endif
