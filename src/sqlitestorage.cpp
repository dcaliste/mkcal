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

  @brief
  This class provides a calendar storage as an sqlite database.

  @author Tero Aho \<ext-tero.1.aho@nokia.com\>
  @author Pertti Luukko \<ext-pertti.luukko@nokia.com\>
*/
#include "sqlitestorage.h"
#include "sqliteformat.h"
#include "logging_p.h"

#include <KCalendarCore/MemoryCalendar>
#include <KCalendarCore/ICalFormat>
using namespace KCalendarCore;

#include <QFileSystemWatcher>

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>

#include <iostream>
using namespace std;

#ifdef Q_OS_UNIX
#include "semaphore_p.h"
#else
#include <QSystemSemaphore>
#endif

using namespace mKCal;

static const QString gChanged(QLatin1String(".changed"));

static const char *createStatements[] =
{
    CREATE_METADATA,
    CREATE_TIMEZONES,
    // Create a global empty entry.
    INSERT_TIMEZONES,
    CREATE_CALENDARS,
    CREATE_COMPONENTS,
    CREATE_RDATES,
    CREATE_CUSTOMPROPERTIES,
    CREATE_RECURSIVE,
    CREATE_ALARM,
    CREATE_ATTENDEE,
    CREATE_ATTACHMENTS,
    CREATE_CALENDARPROPERTIES,
    /* Create index on frequently used columns */
    INDEX_CALENDAR,
    INDEX_COMPONENT,
    INDEX_COMPONENT_UID,
    INDEX_COMPONENT_NOTEBOOK,
    INDEX_RDATES,
    INDEX_CUSTOMPROPERTIES,
    INDEX_RECURSIVE,
    INDEX_ALARM,
    INDEX_ATTENDEE,
    INDEX_ATTACHMENTS,
    INDEX_CALENDARPROPERTIES,
    "PRAGMA foreign_keys = ON"
};

/**
  Private class that helps to provide binary compatibility between releases.
  @internal
*/
//@cond PRIVATE
class mKCal::SqliteStorage::Private
{
public:
    Private(const QTimeZone &timeZone, SqliteStorage *storage,
            const QString &databaseName
           )
        : mTimeZone(timeZone),
          mStorage(storage),
          mDatabaseName(databaseName),
#ifdef Q_OS_UNIX
          mSem(databaseName),
#else
          mSem(databaseName, 1, QSystemSemaphore::Open),
#endif
          mChanged(databaseName + gChanged),
          mWatcher(0),
          mFormat(0)
    {}
    ~Private()
    {
    }

    QTimeZone mTimeZone;
    SqliteStorage *mStorage;
    QString mDatabaseName;
#ifdef Q_OS_UNIX
    ProcessMutex mSem;
#else
    QSystemSemaphore mSem;
#endif

    QFile mChanged;
    QFileSystemWatcher *mWatcher;
    int mSavedTransactionId;
    sqlite3 *mDatabase = nullptr;
    SqliteFormat *mFormat = nullptr;

    int loadIncidences(sqlite3_stmt *stmt1,
                       int limit = -1, QDateTime *last = NULL, bool useDate = false,
                       bool ignoreEnd = false);
    bool saveIncidences(const QMultiHash<QString, Incidence::Ptr> &list, DBOperation dbop);
    bool selectIncidences(Incidence::List *list,
                          const char *query1, int qsize1,
                          DBOperation dbop, const QDateTime &after,
                          const QString &notebookUid, const QString &summary = QString());
    bool purgeDeletedIncidences(const Incidence::List &list);
    int selectCount(const char *query, int qsize);
    bool saveTimezones();
    bool loadTimezones();
};
//@endcond

SqliteStorage::SqliteStorage(const ExtendedCalendar::Ptr &cal, const QString &databaseName,
                             bool validateNotebooks)
    : ExtendedStorage(cal, validateNotebooks),
      d(new Private(cal->timeZone(), this, databaseName))
{
}

// QDir::isReadable() doesn't support group permissions, only user permissions.
static bool directoryIsRW(const QString &dirPath)
{
    QFileInfo databaseDirInfo(dirPath);
    return (databaseDirInfo.permission(QFile::ReadGroup | QFile::WriteGroup)
            || databaseDirInfo.permission(QFile::ReadUser  | QFile::WriteUser));
}

static QString defaultLocation()
{
    // Environment variable is taking precedence.
    QString dbFile = QLatin1String(qgetenv("SQLITESTORAGEDB"));
    if (dbFile.isEmpty()) {
        // Otherwise, use a central storage location by default
        const QString privilegedDataDir = QString("%1/.local/share/system/privileged/").arg(QDir::homePath());

        QDir databaseDir(privilegedDataDir);
        if (databaseDir.exists() && directoryIsRW(privilegedDataDir)) {
            databaseDir = privilegedDataDir + QLatin1String("Calendar/mkcal/");
        } else {
            databaseDir = QString("%1/.local/share/system/Calendar/mkcal/").arg(QDir::homePath());
        }

        if (!databaseDir.exists() && !databaseDir.mkpath(QString::fromLatin1("."))) {
            qCWarning(lcMkcal) << "Unable to create calendar database directory:" << databaseDir.path();
        }

        dbFile = databaseDir.absoluteFilePath(QLatin1String("db"));
    }

    return dbFile;
}

SqliteStorage::SqliteStorage(const ExtendedCalendar::Ptr &cal, bool validateNotebooks)
    : SqliteStorage(cal, defaultLocation(), validateNotebooks)
{
}

SqliteStorage::~SqliteStorage()
{
    close();
    delete d;
}

QString SqliteStorage::databaseName() const
{
    return d->mDatabaseName;
}

bool SqliteStorage::open()
{
    int rv;
    char *errmsg = NULL;
    const char *query = NULL;

    if (d->mDatabase) {
        return false;
    }

    if (!d->mSem.acquire()) {
        qCWarning(lcMkcal) << "cannot lock" << d->mDatabaseName << "error" << d->mSem.errorString();
        return false;
    }

    rv = sqlite3_open(d->mDatabaseName.toUtf8(), &d->mDatabase);
    if (rv) {
        qCWarning(lcMkcal) << "sqlite3_open error:" << rv << "on database" << d->mDatabaseName;
        qCWarning(lcMkcal) << sqlite3_errmsg(d->mDatabase);
        goto error;
    }
    qCDebug(lcMkcal) << "database" << d->mDatabaseName << "opened";

    // Set one and half second busy timeout for waiting for internal sqlite locks
    sqlite3_busy_timeout(d->mDatabase, 1500);

    for (unsigned int i = 0; i < (sizeof(createStatements)/sizeof(createStatements[0])); i++) {
         query = createStatements[i];
         SL3_exec(d->mDatabase);
    }

    d->mFormat = new SqliteFormat(d->mDatabase, d->mTimeZone);
    d->mFormat->selectMetadata(&d->mSavedTransactionId);

    if (!d->mChanged.open(QIODevice::Append)) {
        qCWarning(lcMkcal) << "cannot open changed file for" << d->mDatabaseName;
        goto error;
    }
    d->mWatcher = new QFileSystemWatcher();
    d->mWatcher->addPath(d->mChanged.fileName());
    connect(d->mWatcher, &QFileSystemWatcher::fileChanged,
            this, &SqliteStorage::fileChanged);

    if (!d->mSem.release()) {
        qCWarning(lcMkcal) << "cannot release lock" << d->mDatabaseName << "error" << d->mSem.errorString();
        goto error;
    }

    if (!d->loadTimezones()) {
        qCWarning(lcMkcal) << "cannot load timezones from database";
        close();
        return false;
    }

    if (!ExtendedStorage::open()) {
        close();
        return false;
    }

    return true;

error:
    if (!d->mSem.release()) {
        qCWarning(lcMkcal) << "cannot release lock" << d->mDatabaseName << "error" << d->mSem.errorString();
    }
    close();
    return false;
}

Person::List SqliteStorage::loadContacts()
{
    Person::List list;

    if (!d->mDatabase) {
        return list;
    }

    list = d->mFormat->selectContacts();

    return list;
}

bool SqliteStorage::notifyOpened(const Incidence::Ptr &incidence)
{
    Q_UNUSED(incidence);
    return false;
}

static sqlite3_stmt* _prepare(sqlite3 *database, const char *query, int qsize)
{
    sqlite3_stmt *stmt = NULL;
    int rv;
    SL3_prepare_v2(database, query, qsize, &stmt, NULL);
 error:
    return stmt;
}

int SqliteStorage::loadIncidences(const ExtendedStorage::Filter &filter)
{
    if (!d->mDatabase) {
        return -1;
    }

    int count = -1;
    sqlite3_stmt *stmt1 = NULL;

#define PREPARE(Q) _prepare(d->mDatabase, Q, sizeof(Q))
    switch (filter.type()) {
    case (ExtendedStorage::Filter::None):
        stmt1 = PREPARE(SELECT_COMPONENTS_ALL);
        if (stmt1) {
            count = d->loadIncidences(stmt1);
            sqlite3_finalize(stmt1);
        }
        return count;
    case (ExtendedStorage::Filter::ByNotebook):
        if (!static_cast<const NotebookFilter*>(&filter)->notebookUid().isEmpty()) {
            stmt1 = PREPARE(SELECT_COMPONENTS_BY_NOTEBOOKUID);
            if (stmt1) {
                QByteArray u = static_cast<const NotebookFilter*>(&filter)->notebookUid().toUtf8();
                if (sqlite3_bind_text(stmt1, 1, u.constData(), u.length(), SQLITE_STATIC)) {
                    qCWarning(lcMkcal) << sqlite3_errmsg(d->mDatabase);
                } else {
                    count = d->loadIncidences(stmt1);
                }
                sqlite3_finalize(stmt1);
            }
        }
        return count;
    case (ExtendedStorage::Filter::ByIncidence):
        if (!static_cast<const IncidenceFilter*>(&filter)->uid().isEmpty()) {
            stmt1 = PREPARE(SELECT_COMPONENTS_BY_UID_AND_RECURID);
            if (stmt1) {
                const QByteArray u = static_cast<const IncidenceFilter*>(&filter)->uid().toUtf8();
                const QDateTime recurrenceId = static_cast<const IncidenceFilter*>(&filter)->recurrenceId();
                // no recurrenceId, bind NULL
                // note that sqlite3_bind_null doesn't seem to work here
                // also note that sqlite should bind NULL automatically if nothing
                // is bound, but that doesn't work either
                qint64 secsRecurId = 0;
                if (recurrenceId.isValid()) {
                    if (recurrenceId.timeSpec() == Qt::LocalTime) {
                        secsRecurId = d->mFormat->toLocalOriginTime(recurrenceId);
                    } else {
                        secsRecurId = d->mFormat->toOriginTime(recurrenceId);
                    }
                }
                if (sqlite3_bind_text(stmt1, 1, u.constData(), u.length(), SQLITE_STATIC) ||
                    sqlite3_bind_int64(stmt1, 2, secsRecurId)) {
                    qCWarning(lcMkcal) << sqlite3_errmsg(d->mDatabase);
                } else {
                    count = d->loadIncidences(stmt1);
                }
                sqlite3_finalize(stmt1);
            }
        }
        return count;
    case (ExtendedStorage::Filter::BySeries):
        if (!static_cast<const SeriesFilter*>(&filter)->uid().isEmpty()) {
            stmt1 = PREPARE(SELECT_COMPONENTS_BY_UID);
            if (stmt1) {
                QByteArray u = static_cast<const SeriesFilter*>(&filter)->uid().toUtf8();
                if (sqlite3_bind_text(stmt1, 1, u.constData(), u.length(), SQLITE_STATIC)) {
                    qCWarning(lcMkcal) << sqlite3_errmsg(d->mDatabase);
                } else {
                    count = d->loadIncidences(stmt1);
                }
                sqlite3_finalize(stmt1);
            }
        }
        return count;
    case (ExtendedStorage::Filter::ByDatetimeRange): {
        QDateTime loadStart = static_cast<const RangeFilter*>(&filter)->start();
        qint64 secsStart = d->mFormat->toOriginTime(loadStart);
        QDateTime loadEnd = static_cast<const RangeFilter*>(&filter)->end();
        qint64 secsEnd = d->mFormat->toOriginTime(loadEnd);
        if (loadStart.isValid() && loadEnd.isValid()) {
            stmt1 = PREPARE(SELECT_COMPONENTS_BY_DATE_BOTH);
            if (stmt1 && (sqlite3_bind_int64(stmt1, 1, secsEnd)
                          || sqlite3_bind_int64(stmt1, 2, secsStart)
                          || sqlite3_bind_int64(stmt1, 3, secsStart))) {
                qCWarning(lcMkcal) << sqlite3_errmsg(d->mDatabase);
                sqlite3_finalize(stmt1);
                stmt1 = nullptr;
            }
        } else if (loadStart.isValid()) {
            stmt1 = PREPARE(SELECT_COMPONENTS_BY_DATE_START);
            if (stmt1 && (sqlite3_bind_int64(stmt1, 1, secsStart)
                          || sqlite3_bind_int64(stmt1, 2, secsStart))) {
                qCWarning(lcMkcal) << sqlite3_errmsg(d->mDatabase);
                sqlite3_finalize(stmt1);
                stmt1 = nullptr;
            }
        } else if (loadEnd.isValid()) {
            stmt1 = PREPARE(SELECT_COMPONENTS_BY_DATE_END);
            if (stmt1 && sqlite3_bind_int64(stmt1, 1, secsEnd)) {
                qCWarning(lcMkcal) << sqlite3_errmsg(d->mDatabase);
                sqlite3_finalize(stmt1);
                stmt1 = nullptr;
            }
        } else {
            stmt1 = PREPARE(SELECT_COMPONENTS_ALL);
        }
        if (stmt1) {
            count = d->loadIncidences(stmt1);
            sqlite3_finalize(stmt1);
        }
        return count;
    }
    case (ExtendedStorage::Filter::ByNoDate):
        stmt1 = PREPARE(SELECT_COMPONENTS_BY_PLAIN);
        if (stmt1) {
            count = d->loadIncidences(stmt1);
            sqlite3_finalize(stmt1);
        }
        return count;
    case (ExtendedStorage::Filter::ByTodo):
        stmt1 = PREPARE(SELECT_COMPONENTS_BY_UNCOMPLETED_TODOS);
        if (stmt1) {
            count = d->loadIncidences(stmt1);
            sqlite3_finalize(stmt1);
        }
        return count;
    case (ExtendedStorage::Filter::ByJournal):
        stmt1 = PREPARE(SELECT_COMPONENTS_BY_JOURNAL);
        if (stmt1) {
            count = d->loadIncidences(stmt1);
            sqlite3_finalize(stmt1);
        }
        return count;
    case (ExtendedStorage::Filter::Recursive):
        stmt1 = PREPARE(SELECT_COMPONENTS_BY_RECURSIVE);
        if (stmt1) {
            count = d->loadIncidences(stmt1);
            sqlite3_finalize(stmt1);
        }
        return count;
    case (ExtendedStorage::Filter::ByGeoLocation): {
        float deltaLatitude = static_cast<const GeoLocationFilter*>(&filter)->deltaLatitude();
        float deltaLongitude = static_cast<const GeoLocationFilter*>(&filter)->deltaLongitude();
        if (deltaLatitude >= 180.f && deltaLongitude >= 360.f) {
            stmt1 = PREPARE(SELECT_COMPONENTS_BY_GEO);
        } else {
            float latitude = static_cast<const GeoLocationFilter*>(&filter)->latitude();
            float longitude = static_cast<const GeoLocationFilter*>(&filter)->longitude();
            stmt1 = PREPARE(SELECT_COMPONENTS_BY_GEO_AREA);
            if (stmt1 && (sqlite3_bind_int64(stmt1, 1, latitude - deltaLatitude)
                          || sqlite3_bind_int64(stmt1, 2, longitude - deltaLongitude)
                          || sqlite3_bind_int64(stmt1, 3, latitude + deltaLatitude)
                          || sqlite3_bind_int64(stmt1, 4, longitude + deltaLongitude))) {
                qCWarning(lcMkcal) << sqlite3_errmsg(d->mDatabase);
                sqlite3_finalize(stmt1);
                stmt1 = nullptr;
            }
        }
        if (stmt1) {
            count = d->loadIncidences(stmt1);
            sqlite3_finalize(stmt1);
        }
        return count;
    }
    case (ExtendedStorage::Filter::ByAttendee):
        stmt1 = PREPARE(SELECT_COMPONENTS_BY_ATTENDEE);
        if (stmt1) {
            count = d->loadIncidences(stmt1);
            sqlite3_finalize(stmt1);
        }
        return count;
    default:
        qCWarning(lcMkcal) << "unsupported filter type" << filter.type();
        return count;
    }
}

int SqliteStorage::loadSortedIncidences(const ExtendedStorage::SortedFilter &filter, int limit, QDateTime *last)
{
    if (!d->mDatabase) {
        return -1;
    }

    int count = -1;
    sqlite3_stmt *stmt1 = NULL;
    qint64 secsStart = last && last->isValid() ? d->mFormat->toOriginTime(*last) : LLONG_MAX;

#define PREPARE(Q) _prepare(d->mDatabase, Q, sizeof(Q))
    switch (filter.type()) {
    case (ExtendedStorage::Filter::SortedByDatetime):
        stmt1 = PREPARE(filter.before()
                        ? (filter.useDate() ? SELECT_COMPONENTS_BY_DATE_SMART
                           : SELECT_COMPONENTS_BY_CREATED_SMART)
                        : SELECT_COMPONENTS_BY_FUTURE_DATE_SMART);
        if (stmt1 && !sqlite3_bind_int64(stmt1, 1, secsStart)) {
            qCWarning(lcMkcal) << sqlite3_errmsg(d->mDatabase);
            sqlite3_finalize(stmt1);
            stmt1 = nullptr;
        }
        if (stmt1) {
            if (filter.before()) {
                count = d->loadIncidences(stmt1, limit, last, filter.useDate());
            } else {
                count = d->loadIncidences(stmt1, limit, last, true, true);
            }
            sqlite3_finalize(stmt1);
        }
        return count;
    case (ExtendedStorage::Filter::ByJournal):
        stmt1 = PREPARE(SELECT_COMPONENTS_BY_JOURNAL_DATE);
        if (stmt1 && !sqlite3_bind_int64(stmt1, 1, secsStart)) {
            qCWarning(lcMkcal) << sqlite3_errmsg(d->mDatabase);
            sqlite3_finalize(stmt1);
            stmt1 = nullptr;
        }
        if (stmt1) {
            count = d->loadIncidences(stmt1, limit, last, true);
            sqlite3_finalize(stmt1);
        }
        return count;
    case (ExtendedStorage::Filter::ByTodo):
        stmt1 = PREPARE(filter.useDate()
                        ? SELECT_COMPONENTS_BY_COMPLETED_TODOS_AND_DATE
                        : SELECT_COMPONENTS_BY_COMPLETED_TODOS_AND_CREATED);
        if (stmt1 && !sqlite3_bind_int64(stmt1, 1, secsStart)) {
            qCWarning(lcMkcal) << sqlite3_errmsg(d->mDatabase);
            sqlite3_finalize(stmt1);
            stmt1 = nullptr;
        }
        if (stmt1) {
            count = d->loadIncidences(stmt1, limit, last, filter.useDate());
            sqlite3_finalize(stmt1);
        }
        return count;
    case (ExtendedStorage::Filter::ByGeoLocation):
        stmt1 = PREPARE(filter.useDate() ? SELECT_COMPONENTS_BY_GEO_AND_DATE
                        : SELECT_COMPONENTS_BY_GEO_AND_CREATED);
        if (stmt1 && !sqlite3_bind_int64(stmt1, 1, secsStart)) {
            qCWarning(lcMkcal) << sqlite3_errmsg(d->mDatabase);
            sqlite3_finalize(stmt1);
            stmt1 = nullptr;
        }
        if (stmt1) {
            count = d->loadIncidences(stmt1, limit, last, filter.useDate());
            sqlite3_finalize(stmt1);
        }
        return count;
    case (ExtendedStorage::Filter::ByAttendee): {
        const QString email = static_cast<const AttendeeFilter*>(&filter)->email();
        if (email.isEmpty()) {
            stmt1 = PREPARE(SELECT_COMPONENTS_BY_ATTENDEE_AND_CREATED);
            if (stmt1 && sqlite3_bind_int64(stmt1, 1, secsStart)) {
                qCWarning(lcMkcal) << sqlite3_errmsg(d->mDatabase);
                sqlite3_finalize(stmt1);
                stmt1 = nullptr;
            }
        } else {
            QByteArray e = email.toUtf8();
            stmt1 = PREPARE(SELECT_COMPONENTS_BY_ATTENDEE_EMAIL_AND_CREATED);
            if (stmt1 && (sqlite3_bind_text(stmt1, 1, e, e.length(), NULL)
                          || sqlite3_bind_int64(stmt1, 2, secsStart))) {
                qCWarning(lcMkcal) << sqlite3_errmsg(d->mDatabase);
                sqlite3_finalize(stmt1);
                stmt1 = nullptr;
            }
        }
        if (stmt1) {
            count = d->loadIncidences(stmt1, limit, last, false);
            sqlite3_finalize(stmt1);
        }
        return count;
    }
    default:
        qCWarning(lcMkcal) << "unsupported sorted filter type" << filter.type();
        return count;
    }
}

int SqliteStorage::Private::loadIncidences(sqlite3_stmt *stmt1,
                                           int limit, QDateTime *last,
                                           bool useDate,
                                           bool ignoreEnd)
{
    int count = 0;
    Incidence::Ptr incidence;
    QDateTime previous, date;
    QString notebookUid;

    if (!mSem.acquire()) {
        qCWarning(lcMkcal) << "cannot lock" << mDatabaseName << "error" << mSem.errorString();
        return -1;
    }

    QMultiHash<QString, Incidence::Ptr> incidences;
    while ((incidence = mFormat->selectComponents(stmt1, notebookUid))) {

        const QDateTime endDateTime(incidence->dateTime(Incidence::RoleEnd));
        if (useDate && endDateTime.isValid()
            && (!ignoreEnd || incidence->type() != Incidence::TypeEvent)) {
            date = endDateTime;
        } else if (useDate && incidence->dtStart().isValid()) {
            date = incidence->dtStart();
        } else {
            date = incidence->created();
        }
        if (previous != date) {
            if (!previous.isValid() || limit <= 0 || count <= limit) {
                // If we don't have previous date, or we're within limits,
                // we can just set the 'previous' and move onward
                previous = date;
            } else {
                // Move back to old date
                date = previous;
                break;
            }
        }
        incidences.insert(notebookUid, incidence);
        count += 1;
    }
    if (last) {
        *last = date;
    }

    if (!mSem.release()) {
        qCWarning(lcMkcal) << "cannot release lock" << mDatabaseName << "error" << mSem.errorString();
    }
    mStorage->setLoaded(incidences);
    mStorage->setFinished(false, "load completed");

    return count;
}
//@endcond

bool SqliteStorage::purgeDeletedIncidences(const Incidence::List &list)
{
    if (!d->mDatabase) {
        return false;
    }

    if (!d->mSem.acquire()) {
        qCWarning(lcMkcal) << "cannot lock" << d->mDatabaseName << "error" << d->mSem.errorString();
        return false;
    }

    bool success = d->purgeDeletedIncidences(list);

    if (!d->mSem.release()) {
        qCWarning(lcMkcal) << "cannot release lock" << d->mDatabaseName << "error" << d->mSem.errorString();
    }

    return success;
}

//@cond PRIVATE
bool SqliteStorage::Private::purgeDeletedIncidences(const Incidence::List &list)
{
    int rv = 0;
    unsigned int error = 1;

    char *errmsg = NULL;
    const char *query = NULL;

    query = BEGIN_TRANSACTION;
    SL3_exec(mDatabase);

    error = 0;
    for (const Incidence::Ptr &incidence: list) {
        if (!mFormat->purgeDeletedComponents(*incidence)) {
            error += 1;
        }
    }

    query = COMMIT_TRANSACTION;
    SL3_exec(mDatabase);

    return !error;

 error:
    qCWarning(lcMkcal) << "Sqlite error:" << sqlite3_errmsg(mDatabase);
    return false;
}
//@endcond

bool SqliteStorage::storeIncidences(const QMultiHash<QString, Incidence::Ptr> &additions,
                                    const QMultiHash<QString, Incidence::Ptr> &modifications,
                                    const QMultiHash<QString, Incidence::Ptr> &deletions,
                                    ExtendedStorage::DeleteAction deleteAction)
{
    if (!d->mDatabase) {
        return false;
    }

    if (!d->mSem.acquire()) {
        qCWarning(lcMkcal) << "cannot lock" << d->mDatabaseName << "error" << d->mSem.errorString();
        return false;
    }

    if (!d->saveTimezones()) {
        qCWarning(lcMkcal) << "saving timezones failed";
    }

    bool success = true;
    if (!additions.isEmpty() && !d->saveIncidences(additions, DBInsert)) {
        success = false;
    }
    if (!modifications.isEmpty() && !d->saveIncidences(modifications, DBUpdate)) {
        success = false;
    }
    if (deleteAction == ExtendedStorage::MarkDeleted
        && !deletions.isEmpty() && !d->saveIncidences(deletions, DBMarkDeleted)) {
        success = false;
    }
    if (deleteAction == ExtendedStorage::PurgeDeleted
        && !deletions.isEmpty() && !d->saveIncidences(deletions, DBDelete)) {
        success = false;
    }

    bool changed = (d->mTimeZone.isValid() || !additions.isEmpty()
                    || !modifications.isEmpty() || !deletions.isEmpty());
    if (changed)
        d->mFormat->incrementTransactionId(&d->mSavedTransactionId);

    if (!d->mSem.release()) {
        qCWarning(lcMkcal) << "cannot release lock" << d->mDatabaseName << "error" << d->mSem.errorString();
    }

    if (changed) {
        setUpdated(additions.values().toVector(), modifications.values().toVector(), deletions.values().toVector());
        d->mChanged.resize(0);   // make a change to create signal
    }

    setFinished(!success, success ? "save completed" : "errors saving incidences");

    return success;
}

//@cond PRIVATE
bool SqliteStorage::Private::saveIncidences(const QMultiHash<QString, Incidence::Ptr> &list, DBOperation dbop)
{
    int rv = 0;
    int errors = 0;
    const char *operation = (dbop == DBInsert) ? "inserting" :
                            (dbop == DBUpdate) ? "updating" : "deleting";
    QHash<QString, Incidence::Ptr>::const_iterator it;
    char *errmsg = NULL;
    const char *query = NULL;

    query = BEGIN_TRANSACTION;
    SL3_exec(mDatabase);

    for (it = list.constBegin(); it != list.constEnd(); ++it) {
        const QString &notebookUid = it.key();

        // lastModified is a public field of iCal RFC, so user should be
        // able to set its value to arbitrary date and time. This field is
        // updated automatically at each incidence modification already by
        // ExtendedCalendar::incidenceUpdated(). We're just ensuring that
        // the lastModified is valid and set it if not.
        if (!(*it)->lastModified().isValid()) {
            (*it)->setLastModified(QDateTime::currentDateTimeUtc());
        }
        if (dbop == DBInsert && (*it)->created().isNull()) {
            (*it)->setCreated(QDateTime::currentDateTimeUtc());
        }
        qCDebug(lcMkcal) << operation << "incidence" << (*it)->uid() << "notebook" << notebookUid;
        if (!mFormat->modifyComponents(**it, notebookUid, dbop)) {
            qCWarning(lcMkcal) << sqlite3_errmsg(mDatabase) << "for incidence" << (*it)->uid();
            errors++;
        } else  if (dbop == DBInsert) {
            // Don't leave deleted events with the same UID/recID.
            if (!mFormat->purgeDeletedComponents(**it)) {
                qCWarning(lcMkcal) << "cannot purge deleted components on insertion.";
                errors += 1;
            }
        }
    }

    // TODO What if there were errors? Options: 1) rollback 2) best effort.

    query = COMMIT_TRANSACTION;
    SL3_exec(mDatabase);

    return errors == 0;

error:
    return false;
}
//@endcond

bool SqliteStorage::cancel()
{
    return true;
}

bool SqliteStorage::close()
{
    if (d->mDatabase) {
        if (d->mWatcher) {
            d->mWatcher->removePaths(d->mWatcher->files());
            // This should work, as storage should be closed before
            // application terminates now. If not, deadlock occurs.
            delete d->mWatcher;
            d->mWatcher = NULL;
        }
        d->mChanged.close();
        delete d->mFormat;
        d->mFormat = 0;
        sqlite3_close(d->mDatabase);
        d->mDatabase = 0;
    }
    return ExtendedStorage::close();
}

//@cond PRIVATE
bool SqliteStorage::Private::selectIncidences(Incidence::List *list,
                                              const char *query1, int qsize1,
                                              DBOperation dbop, const QDateTime &after,
                                              const QString &notebookUid, const QString &summary)
{
    int rv = 0;
    sqlite3_stmt *stmt1 = NULL;
    int index;
    QByteArray n;
    QByteArray s;
    Incidence::Ptr incidence;
    sqlite3_int64 secs;
    QString nbook;

    if (!mSem.acquire()) {
        qCWarning(lcMkcal) << "cannot lock" << mDatabaseName << "error" << mSem.errorString();
        return false;
    }

    SL3_prepare_v2(mDatabase, query1, qsize1, &stmt1, nullptr);

    qCDebug(lcMkcal) << "incidences"
             << (dbop == DBInsert ? "inserted" :
                 dbop == DBUpdate ? "updated" :
                 dbop == DBMarkDeleted ? "deleted" : "")
             << "since" << after.toString();

    if (query1) {
        if (after.isValid()) {
            if (dbop == DBInsert) {
                index = 1;
                secs = mFormat->toOriginTime(after);
                SL3_bind_int64(stmt1, index, secs);
                if (!notebookUid.isNull()) {
                    index = 2;
                    n = notebookUid.toUtf8();
                    SL3_bind_text(stmt1, index, n.constData(), n.length(), SQLITE_STATIC);
                }
            }
            if (dbop == DBUpdate || dbop == DBMarkDeleted) {
                index = 1;
                secs = mFormat->toOriginTime(after);
                SL3_bind_int64(stmt1, index, secs);
                index = 2;
                SL3_bind_int64(stmt1, index, secs);
                if (!notebookUid.isNull()) {
                    index = 3;
                    n = notebookUid.toUtf8();
                    SL3_bind_text(stmt1, index, n.constData(), n.length(), SQLITE_STATIC);
                }
            }
            if (dbop == DBSelect) {
                index = 1;
                secs = mFormat->toOriginTime(after);
                qCDebug(lcMkcal) << "QUERY FROM" << secs;
                SL3_bind_int64(stmt1, index, secs);
                index = 2;
                s = summary.toUtf8();
                SL3_bind_text(stmt1, index, s.constData(), s.length(), SQLITE_STATIC);
                if (!notebookUid.isNull()) {
                    qCDebug(lcMkcal) << "notebook" << notebookUid.toUtf8().constData();
                    index = 3;
                    n = notebookUid.toUtf8();
                    SL3_bind_text(stmt1, index, n.constData(), n.length(), SQLITE_STATIC);
                }
            }
        } else {
            if (!notebookUid.isNull()) {
                index = 1;
                n = notebookUid.toUtf8();
                SL3_bind_text(stmt1, index, n.constData(), n.length(), SQLITE_STATIC);
            }
        }
    }

    while ((incidence = mFormat->selectComponents(stmt1, nbook))) {
        qCDebug(lcMkcal) << "adding incidence" << incidence->uid() << "into list"
                 << incidence->created() << incidence->lastModified();
        list->append(incidence);
    }
    sqlite3_finalize(stmt1);

    if (!mSem.release()) {
        qCWarning(lcMkcal) << "cannot release lock" << mDatabaseName << "error" << mSem.errorString();
    }
    mStorage->setFinished(false, "select completed");
    return true;

error:
    if (!mSem.release()) {
        qCWarning(lcMkcal) << "cannot release lock" << mDatabaseName << "error" << mSem.errorString();
    }
    mStorage->setFinished(true, "error selecting incidences");
    return false;
}
//@endcond

bool SqliteStorage::insertedIncidences(Incidence::List *list, const QDateTime &after,
                                       const QString &notebookUid)
{
    if (d->mDatabase && list && after.isValid()) {
        const char *query1 = NULL;
        int qsize1 = 0;

        if (!notebookUid.isNull()) {
            query1 = SELECT_COMPONENTS_BY_CREATED_AND_NOTEBOOK;
            qsize1 = sizeof(SELECT_COMPONENTS_BY_CREATED_AND_NOTEBOOK);
        } else {
            query1 = SELECT_COMPONENTS_BY_CREATED;
            qsize1 = sizeof(SELECT_COMPONENTS_BY_CREATED);
        }

        return d->selectIncidences(list, query1, qsize1,
                                   DBInsert, after, notebookUid);
    }
    return false;
}

bool SqliteStorage::modifiedIncidences(Incidence::List *list, const QDateTime &after,
                                       const QString &notebookUid)
{
    if (d->mDatabase && list && after.isValid()) {
        const char *query1 = NULL;
        int qsize1 = 0;

        if (!notebookUid.isNull()) {
            query1 = SELECT_COMPONENTS_BY_LAST_MODIFIED_AND_NOTEBOOK;
            qsize1 = sizeof(SELECT_COMPONENTS_BY_LAST_MODIFIED_AND_NOTEBOOK);
        } else {
            query1 = SELECT_COMPONENTS_BY_LAST_MODIFIED;
            qsize1 = sizeof(SELECT_COMPONENTS_BY_LAST_MODIFIED);
        }

        return d->selectIncidences(list, query1, qsize1,
                                   DBUpdate, after, notebookUid);
    }
    return false;
}

bool SqliteStorage::deletedIncidences(Incidence::List *list, const QDateTime &after,
                                      const QString &notebookUid)
{
    if (d->mDatabase && list) {
        const char *query1 = NULL;
        int qsize1 = 0;

        if (!notebookUid.isNull()) {
            if (after.isValid()) {
                query1 = SELECT_COMPONENTS_BY_DELETED_AND_NOTEBOOK;
                qsize1 = sizeof(SELECT_COMPONENTS_BY_DELETED_AND_NOTEBOOK);
            } else {
                query1 = SELECT_COMPONENTS_ALL_DELETED_BY_NOTEBOOK;
                qsize1 = sizeof(SELECT_COMPONENTS_ALL_DELETED_BY_NOTEBOOK);
            }
        } else {
            if (after.isValid()) {
                query1 = SELECT_COMPONENTS_BY_DELETED;
                qsize1 = sizeof(SELECT_COMPONENTS_BY_DELETED);
            } else {
                query1 = SELECT_COMPONENTS_ALL_DELETED;
                qsize1 = sizeof(SELECT_COMPONENTS_ALL_DELETED);
            }
        }

        return d->selectIncidences(list, query1, qsize1,
                                   DBMarkDeleted, after, notebookUid);
    }
    return false;
}

bool SqliteStorage::allIncidences(Incidence::List *list, const QString &notebookUid)
{
    if (d->mDatabase && list) {
        const char *query1 = NULL;
        int qsize1 = 0;

        if (!notebookUid.isNull()) {
            query1 = SELECT_COMPONENTS_BY_NOTEBOOK;
            qsize1 = sizeof(SELECT_COMPONENTS_BY_NOTEBOOK);
        } else {
            query1 = SELECT_COMPONENTS_ALL;
            qsize1 = sizeof(SELECT_COMPONENTS_ALL);
        }

        return d->selectIncidences(list, query1, qsize1,
                                   DBSelect, QDateTime(), notebookUid);
    }
    return false;
}

bool SqliteStorage::duplicateIncidences(Incidence::List *list, const Incidence::Ptr &incidence,
                                        const QString &notebookUid)
{
    if (d->mDatabase && list && incidence) {
        const char *query1 = NULL;
        int qsize1 = 0;
        QDateTime dtStart;

        if (incidence->dtStart().isValid()) {
            dtStart = incidence->dtStart();
        } else {
            dtStart = QDateTime();
        }

        if (!notebookUid.isNull()) {
            query1 = SELECT_COMPONENTS_BY_DUPLICATE_AND_NOTEBOOK;
            qsize1 = sizeof(SELECT_COMPONENTS_BY_DUPLICATE_AND_NOTEBOOK);
        } else {
            query1 = SELECT_COMPONENTS_BY_DUPLICATE;
            qsize1 = sizeof(SELECT_COMPONENTS_BY_DUPLICATE);
        }

        return d->selectIncidences(list, query1, qsize1,
                                   DBSelect, dtStart, notebookUid, incidence->summary());
    }
    return false;

}

QDateTime SqliteStorage::incidenceDeletedDate(const Incidence::Ptr &incidence)
{
    int index;
    QByteArray u;
    int rv = 0;
    sqlite3_int64 date;
    QDateTime deletionDate;

    if (!d->mDatabase) {
        return deletionDate;
    }

    const char *query = SELECT_COMPONENTS_BY_UID_RECID_AND_DELETED;
    int qsize = sizeof(SELECT_COMPONENTS_BY_UID_RECID_AND_DELETED);
    sqlite3_stmt *stmt = NULL;
    const char *tail = NULL;

    SL3_prepare_v2(d->mDatabase, query, qsize, &stmt, &tail);
    index = 1;
    u = incidence->uid().toUtf8();
    SL3_bind_text(stmt, index, u.constData(), u.length(), SQLITE_STATIC);
    if (incidence->hasRecurrenceId()) {
        qint64 secsRecurId;
        if (incidence->recurrenceId().timeSpec() == Qt::LocalTime) {
            secsRecurId = d->mFormat->toLocalOriginTime(incidence->recurrenceId());
        } else {
            secsRecurId = d->mFormat->toOriginTime(incidence->recurrenceId());
        }
        SL3_bind_int64(stmt, index, secsRecurId);
    } else {
        SL3_bind_int64(stmt, index, 0);
    }

    if (!d->mSem.acquire()) {
        qCWarning(lcMkcal) << "cannot lock" << d->mDatabaseName << "error" << d->mSem.errorString();
        return deletionDate;
    }

    SL3_step(stmt);
    if ((rv == SQLITE_ROW) || (rv == SQLITE_OK)) {
        date = sqlite3_column_int64(stmt, 1);
        deletionDate = d->mFormat->fromOriginTime(date);
    }

error:
    sqlite3_finalize(stmt);

    if (!d->mSem.release()) {
        qCWarning(lcMkcal) << "cannot release lock" << d->mDatabaseName << "error" << d->mSem.errorString();
    }
    return deletionDate;
}

//@cond PRIVATE
int SqliteStorage::Private::selectCount(const char *query, int qsize)
{
    int rv = 0;
    int count = 0;
    sqlite3_stmt *stmt = NULL;
    const char *tail = NULL;

    if (!mDatabase) {
        return count;
    }

    if (!mSem.acquire()) {
        qCWarning(lcMkcal) << "cannot lock" << mDatabaseName << "error" << mSem.errorString();
        return count;
    }

    SL3_prepare_v2(mDatabase, query, qsize, &stmt, &tail);
    SL3_step(stmt);
    if ((rv == SQLITE_ROW) || (rv == SQLITE_OK)) {
        count = sqlite3_column_int(stmt, 0);
    }

error:
    sqlite3_reset(stmt);
    sqlite3_finalize(stmt);

    if (!mSem.release()) {
        qCWarning(lcMkcal) << "cannot release lock" << mDatabaseName << "error" << mSem.errorString();
    }
    return count;
}
//@endcond

int SqliteStorage::eventCount()
{
    const char *query = SELECT_EVENT_COUNT;
    int qsize = sizeof(SELECT_EVENT_COUNT);

    return d->selectCount(query, qsize);
}

int SqliteStorage::todoCount()
{
    const char *query = SELECT_TODO_COUNT;
    int qsize = sizeof(SELECT_TODO_COUNT);

    return d->selectCount(query, qsize);
}

int SqliteStorage::journalCount()
{
    const char *query = SELECT_JOURNAL_COUNT;
    int qsize = sizeof(SELECT_JOURNAL_COUNT);

    return d->selectCount(query, qsize);
}

bool SqliteStorage::loadNotebooks(QList<Notebook> *notebooks, QString *defaultNotebookId)
{
    if (!d->mDatabase) {
        return false;
    }

    if (!d->mSem.acquire()) {
        qCWarning(lcMkcal) << "cannot lock" << d->mDatabaseName << "error" << d->mSem.errorString();
        return false;
    }

    bool isDefault;
    notebooks->clear();
    for (Notebook nb = d->mFormat->selectCalendars(&isDefault); nb.isValid();
         nb = d->mFormat->selectCalendars(&isDefault)) {
        qCDebug(lcMkcal) << "loaded notebook" << nb.uid() << nb.name() << "from database";
        if (isDefault) {
            *defaultNotebookId = nb.uid();
        }
        notebooks->append(nb);
    }

    if (!d->mSem.release()) {
        qCWarning(lcMkcal) << "cannot release lock" << d->mDatabaseName << "error" << d->mSem.errorString();
    }

    return true;
}

bool SqliteStorage::modifyNotebook(const Notebook &nb, DBOperation dbop)
{
    bool success = false;

    if (!d->mDatabase) {
        return false;
    }

    Incidence::List deleted;
    Incidence::List all;
    if (dbop == DBDelete) {
        deletedIncidences(&deleted, QDateTime(), nb.uid());
        allIncidences(&all, nb.uid());
    }

    if (!d->mSem.acquire()) {
        qCWarning(lcMkcal) << "cannot lock" << d->mDatabaseName << "error" << d->mSem.errorString();
        return false;
    }

    success = d->mFormat->modifyCalendars(nb, dbop, nb.uid() == defaultNotebookId());

    // Don't leave orphaned incidences behind.
    if (success && !deleted.isEmpty()) {
        qCDebug(lcMkcal) << "purging" << deleted.count() << "incidences of notebook" << nb.name();
        if (!d->purgeDeletedIncidences(deleted)) {
            qCWarning(lcMkcal) << "error when purging deleted incidences from notebook" << nb.uid();
        }
    }
    if (success && !all.isEmpty()) {
        qCDebug(lcMkcal) << "deleting" << all.size() << "incidences of notebook" << nb.name();
        QMultiHash<QString, Incidence::Ptr> deletions;
        for (const Incidence::Ptr &incidence : all) {
            deletions.insert(nb.uid(), incidence);
        }
        if (!d->saveIncidences(deletions, DBDelete)) {
            qCWarning(lcMkcal) << "error when purging incidences from notebook" << nb.uid();
        }
    }

    if (success) {
        // Don't save the incremented transactionId at the moment,
        // let it be seen as an external modification.
        // Todo: add a method for observers on notebook changes.
        if (!d->mFormat->incrementTransactionId(nullptr))
            d->mSavedTransactionId = -1;
    }

    if (!d->mSem.release()) {
        qCWarning(lcMkcal) << "cannot release lock" << d->mDatabaseName << "error" << d->mSem.errorString();
    }

    if (success) {
        d->mChanged.resize(0);   // make a change to create signal
    }

    return success;
}

bool SqliteStorage::Private::saveTimezones()
{
    int rv = 0;
    int index = 1;

    if (!mDatabase) {
        return false;
    }

    const char *query1 = UPDATE_TIMEZONES;
    int qsize1 = sizeof(UPDATE_TIMEZONES);
    sqlite3_stmt *stmt1 = NULL;

    if (mTimeZone.isValid()) {
        MemoryCalendar::Ptr temp(new MemoryCalendar(mTimeZone));
        ICalFormat ical;
        QByteArray data = ical.toString(temp, QString()).toUtf8();

        // Semaphore is already locked here.
        SL3_prepare_v2(mDatabase, query1, qsize1, &stmt1, NULL);
        SL3_bind_text(stmt1, index, data, data.length(), SQLITE_STATIC);
        SL3_step(stmt1);
        qCDebug(lcMkcal) << "updated timezones in database";
        sqlite3_finalize(stmt1);
    }

    return true;

 error:
    sqlite3_finalize(stmt1);
    qCWarning(lcMkcal) << sqlite3_errmsg(mDatabase);

    return false;
}

bool SqliteStorage::Private::loadTimezones()
{
    int rv = 0;
    bool success = false;

    if (!mDatabase) {
        return false;
    }

    const char *query = SELECT_TIMEZONES;
    int qsize = sizeof(SELECT_TIMEZONES);
    sqlite3_stmt *stmt = NULL;
    const char *tail = NULL;

    SL3_prepare_v2(mDatabase, query, qsize, &stmt, &tail);

    if (!mSem.acquire()) {
        qCWarning(lcMkcal) << "cannot lock" << mDatabaseName << "error" << mSem.errorString();
        return false;
    }

    SL3_step(stmt);
    if (rv == SQLITE_ROW) {
        QString zoneData = QString::fromUtf8((const char *)sqlite3_column_text(stmt, 1));
        if (!zoneData.isEmpty()) {
            MemoryCalendar::Ptr temp(new MemoryCalendar(mTimeZone));
            ICalFormat ical;
            if (!ical.fromString(temp, zoneData)) {
                qCWarning(lcMkcal) << "failed to load timezones from database";
                mTimeZone = QTimeZone();
            }
        }
    }
    // Return true in any case, unless there was an sql error.
    success = true;

error:
    sqlite3_reset(stmt);
    sqlite3_finalize(stmt);

    if (!mSem.release()) {
        qCWarning(lcMkcal) << "cannot release lock" << mDatabaseName << "error" << mSem.errorString();
    }
    return success;
}

QTimeZone SqliteStorage::timeZone() const
{
    return d->mTimeZone;
}

void SqliteStorage::fileChanged(const QString &path)
{
    if (!d->mSem.acquire()) {
        qCWarning(lcMkcal) << "cannot lock" << d->mDatabaseName << "error" << d->mSem.errorString();
        return;
    }
    int transactionId;
    if (!d->mFormat->selectMetadata(&transactionId))
        transactionId = d->mSavedTransactionId - 1; // Ensure reload on error
    if (!d->mSem.release()) {
        qCWarning(lcMkcal) << "cannot release lock" << d->mDatabaseName << "error" << d->mSem.errorString();
    }

    if (transactionId != d->mSavedTransactionId) {
        d->mSavedTransactionId = transactionId;
        if (!d->loadTimezones()) {
            qCWarning(lcMkcal) << "loading timezones failed";
        }
        setModified(path);
        qCDebug(lcMkcal) << path << "has been modified";
    }
}

void SqliteStorage::virtual_hook(int id, void *data)
{
    Q_UNUSED(id);
    Q_UNUSED(data);
    Q_ASSERT(false);
}
