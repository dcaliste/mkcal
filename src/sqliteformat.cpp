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
  defines the SqliteFormat class.

  @brief
  Sqlite format implementation.

  @author Tero Aho \<ext-tero.1.aho@nokia.com\>
  @author Pertti Luukko \<ext-pertti.luukko@nokia.com\>
*/
#include "sqliteformat.h"
#include "sqlitestorage.h"
#include "logging_p.h"

#include <KCalendarCore/Alarm>
#include <KCalendarCore/Attendee>
#include <KCalendarCore/Person>
#include <KCalendarCore/Sorting>

using namespace KCalendarCore;

#define FLOATING_DATE "FloatingDate"

using namespace mKCal;
class mKCal::SqliteFormat::Private
{
public:
    Private(SqliteStorage *storage, sqlite3 *database)
        : mStorage(storage), mDatabase(database)
        , mSelectCalProps(nullptr)
        , mInsertCalProps(nullptr)
    {
    }
    ~Private()
    {
        sqlite3_finalize(mSelectCalProps);
        sqlite3_finalize(mInsertCalProps);
    }
    SqliteStorage *mStorage;
    sqlite3 *mDatabase;

    // Cache for various queries.
    sqlite3_stmt *mSelectCalProps;
    sqlite3_stmt *mInsertCalProps;

    bool selectCustomproperties(Incidence::Ptr incidence, int rowid, sqlite3_stmt *stmt);
    int selectRowId(Incidence::Ptr incidence);
    bool selectRecursives(Incidence::Ptr incidence, int rowid, sqlite3_stmt *stmt);
    bool selectAlarms(Incidence::Ptr incidence, int rowid, sqlite3_stmt *stmt);
    bool selectAttendees(Incidence::Ptr incidence, int rowid, sqlite3_stmt *stmt);
    bool selectRdates(Incidence::Ptr incidence, int rowid, sqlite3_stmt *stmt);
    bool selectAttachments(Incidence::Ptr incidence, int rowid, sqlite3_stmt *stmt);
    bool selectCalendarProperties(Notebook::Ptr notebook);
    bool modifyCustomproperties(Incidence::Ptr incidence, int rowid, DBOperation dbop,
                                sqlite3_stmt *stmt1, sqlite3_stmt *stmt2);
    bool modifyCustomproperty(int rowid, const QByteArray &key, const QString &value,
                              const QString &parameters, DBOperation dbop, sqlite3_stmt *stmt);
    bool modifyAttendees(Incidence::Ptr incidence, int rowid, DBOperation dbop,
                         sqlite3_stmt *stmt1, sqlite3_stmt *stmt2);
    bool modifyAttendee(int rowid, const Attendee &attendee, DBOperation dbop, sqlite3_stmt *stmt,
                        bool isOrganizer);
    bool modifyAttachments(Incidence::Ptr incidence, int rowid, DBOperation dbop,
                           sqlite3_stmt *deleteStatement, sqlite3_stmt *insertStatement);
    bool modifyAlarms(Incidence::Ptr incidence, int rowid, DBOperation dbop, sqlite3_stmt *stmt1,
                      sqlite3_stmt *stmt2);
    bool modifyAlarm(int rowid, Alarm::Ptr alarm, DBOperation dbop, sqlite3_stmt *stmt);
    bool modifyRecursives(Incidence::Ptr incidence, int rowid, DBOperation dbop,
                          sqlite3_stmt *stmt1, sqlite3_stmt *stmt2);
    bool modifyRecursive(int rowid, RecurrenceRule *rule, DBOperation dbop, sqlite3_stmt *stmt,
                         const int &type);
    bool modifyRdates(Incidence::Ptr incidence, int rowid, DBOperation dbop, sqlite3_stmt *stmt1,
                      sqlite3_stmt *stmt2);
    bool modifyRdate(int rowid, int type, const QDateTime &rdate, bool allDay,
                     DBOperation dbop, sqlite3_stmt *stmt);
    bool modifyCalendarProperties(Notebook::Ptr notebook, DBOperation dbop);
    bool deleteCalendarProperties(const QByteArray &id);
    bool insertCalendarProperty(const QByteArray &id, const QByteArray &key,
                                const QByteArray &value);
};
//@endcond

SqliteFormat::SqliteFormat(SqliteStorage *storage, sqlite3 *database)
    : d(new Private(storage, database))
{
}

SqliteFormat::~SqliteFormat()
{
    delete d;
}

bool SqliteFormat::modifyCalendars(const Notebook::Ptr &notebook,
                                   DBOperation dbop, sqlite3_stmt *stmt)
{
    int rv = 0;
    int index = 1;
    QByteArray uid = notebook->uid().toUtf8();
    QByteArray name = notebook->name().toUtf8();
    QByteArray description = notebook->description().toUtf8();
    QByteArray color = notebook->color().toUtf8();
    QByteArray plugin = notebook->pluginName().toUtf8();
    QByteArray account = notebook->account().toUtf8();
    QByteArray sharedWith = notebook->sharedWithStr().toUtf8();
    QByteArray syncProfile = notebook->syncProfile().toUtf8();

    sqlite3_int64  secs;

    if (dbop == DBInsert || dbop == DBDelete)
        SL3_bind_text(stmt, index, uid, uid.length(), SQLITE_STATIC);

    if (dbop == DBInsert || dbop == DBUpdate) {
        SL3_bind_text(stmt, index, name, name.length(), SQLITE_STATIC);
        SL3_bind_text(stmt, index, description, description.length(), SQLITE_STATIC);
        SL3_bind_text(stmt, index, color, color.length(), SQLITE_STATIC);
        SL3_bind_int(stmt, index, notebook->flags());
        secs = d->mStorage->toOriginTime(notebook->syncDate().toUTC());
        SL3_bind_int64(stmt, index, secs);
        SL3_bind_text(stmt, index, plugin, plugin.length(), SQLITE_STATIC);
        SL3_bind_text(stmt, index, account, account.length(), SQLITE_STATIC);
        SL3_bind_int64(stmt, index, notebook->attachmentSize());
        secs = d->mStorage->toOriginTime(notebook->modifiedDate().toUTC());
        SL3_bind_int64(stmt, index, secs);
        SL3_bind_text(stmt, index, sharedWith, sharedWith.length(), SQLITE_STATIC);
        SL3_bind_text(stmt, index, syncProfile, syncProfile.length(), SQLITE_STATIC);
        secs = d->mStorage->toOriginTime(notebook->creationDate().toUTC());
        SL3_bind_int64(stmt, index, secs);

        if (dbop == DBUpdate)
            SL3_bind_text(stmt, index, uid, uid.length(), SQLITE_STATIC);
    }

    SL3_step(stmt);

    if (!d->modifyCalendarProperties(notebook, dbop)) {
        qCWarning(lcMkcal) << "failed to modify calendarproperties for notebook" << uid;
    }

    return true;

error:
    return false;
}

bool SqliteFormat::purgeDeletedComponents(const KCalendarCore::Incidence::Ptr &incidence,
                                          sqlite3_stmt *stmt1, sqlite3_stmt *stmt2,
                                          sqlite3_stmt *stmt3, sqlite3_stmt *stmt4,
                                          sqlite3_stmt *stmt5, sqlite3_stmt *stmt6,
                                          sqlite3_stmt *stmt7, sqlite3_stmt *attachmentStmt)
{
    int rv;
    int index = 1;
    const QByteArray u(incidence->uid().toUtf8());
    qint64 secsRecurId = 0;

    if (incidence->hasRecurrenceId() && incidence->recurrenceId().timeSpec() == Qt::LocalTime) {
        secsRecurId = d->mStorage->toLocalOriginTime(incidence->recurrenceId());
    } else if (incidence->hasRecurrenceId()) {
        secsRecurId = d->mStorage->toOriginTime(incidence->recurrenceId());
    }

    SL3_bind_text(stmt1, index, u.constData(), u.length(), SQLITE_STATIC);
    SL3_bind_int64(stmt1, index, secsRecurId);

    SL3_step(stmt1);
    while (rv == SQLITE_ROW) {
        int rowid = sqlite3_column_int(stmt1, 0);

        int index2 = 1;
        SL3_bind_int(stmt2, index2, rowid);
        SL3_step(stmt2);
        sqlite3_reset(stmt2);

        if (!d->modifyCustomproperties(incidence, rowid, DBDelete, stmt3, NULL))
            qCWarning(lcMkcal) << "failed to delete customproperties for incidence" << u;

        if (!d->modifyAlarms(incidence, rowid, DBDelete, stmt4, NULL))
            qCWarning(lcMkcal) << "failed to delete alarms for incidence" << u;

        if (!d->modifyAttendees(incidence, rowid, DBDelete, stmt5, NULL))
            qCWarning(lcMkcal) << "failed to delete attendees for incidence" << u;

        if (!d->modifyRecursives(incidence, rowid, DBDelete, stmt6, NULL))
            qCWarning(lcMkcal) << "failed to delete recursives for incidence" << u;

        if (!d->modifyRdates(incidence, rowid, DBDelete, stmt7, NULL))
            qCWarning(lcMkcal) << "failed to delete rdates for incidence" << u;

        if (!d->modifyAttachments(incidence, rowid, DBDelete, attachmentStmt, NULL))
            qCWarning(lcMkcal) << "failed to delete attachments for incidence" << u;

        SL3_step(stmt1);
    }

    sqlite3_reset(stmt1);
    return true;

error:
    return false;
}

static bool setDateTime(SqliteStorage *storage, sqlite3_stmt *stmt, int &index, const QDateTime &dateTime, bool allDay)
{
    int rv = 0;
    sqlite3_int64 secs;
    QByteArray tz;

    if (dateTime.isValid()) {
        secs = (dateTime.timeSpec() == Qt::LocalTime || allDay)
            ? storage->toLocalOriginTime(dateTime) : storage->toOriginTime(dateTime);
        SL3_bind_int64(stmt, index, secs);
        secs = storage->toLocalOriginTime(dateTime);
        SL3_bind_int64(stmt, index, secs);
        if (allDay) {
            tz = FLOATING_DATE;
        } else if (dateTime.timeSpec() != Qt::LocalTime) {
            tz = dateTime.timeZone().id();
        }
        SL3_bind_text(stmt, index, tz.constData(), tz.length(), SQLITE_TRANSIENT);
    } else {
        SL3_bind_int(stmt, index, 0);
        SL3_bind_int(stmt, index, 0);
        SL3_bind_text(stmt, index, "", 0, SQLITE_STATIC);
    }
    return true;
 error:
    return false;
}

#define SL3_bind_date_time( storage, stmt, index, dt, allDay)          \
    {                                                                  \
        if (!setDateTime(storage, stmt, index, dt, allDay))            \
            goto error;                                                \
    }

bool SqliteFormat::modifyComponents(const Incidence::Ptr &incidence, const QString &nbook,
                                    DBOperation dbop,
                                    sqlite3_stmt *stmt1, sqlite3_stmt *stmt2, sqlite3_stmt *stmt3,
                                    sqlite3_stmt *stmt4, sqlite3_stmt *stmt5, sqlite3_stmt *stmt6,
                                    sqlite3_stmt *stmt7, sqlite3_stmt *stmt8, sqlite3_stmt *stmt9,
                                    sqlite3_stmt *stmt10, sqlite3_stmt *stmt11,
                                    sqlite3_stmt *delAttachmentStmt, sqlite3_stmt *insAttachmentStmt)
{
    int rv = 0;
    int index = 1;
    QByteArray uid;
    QByteArray notebook;
    QByteArray type;
    QByteArray summary;
    QByteArray category;
    QByteArray location;
    QByteArray description;
    QByteArray url;
    QByteArray contact;
    QByteArray relatedtouid;
    QByteArray colorstr;
    QByteArray comments;
    QByteArray resources;
    QDateTime dt;
    sqlite3_int64 secs;
    int rowid = 0;

    if (dbop == DBDelete || dbop == DBMarkDeleted || dbop == DBUpdate) {
        rowid = d->selectRowId(incidence);
        if (!rowid) {
            qCWarning(lcMkcal) << "failed to select rowid of incidence" << incidence->uid() << incidence->recurrenceId();
            goto error;
        }
    }

    if (dbop == DBDelete) {
        SL3_bind_int(stmt1, index, rowid);
    }

    if (dbop == DBMarkDeleted) {
        secs = d->mStorage->toOriginTime(QDateTime::currentDateTimeUtc());
        SL3_bind_int64(stmt1, index, secs);
        SL3_bind_int(stmt1, index, rowid);
    }

    if (dbop == DBInsert || dbop == DBUpdate) {
        notebook = nbook.toUtf8();
        SL3_bind_text(stmt1, index, notebook.constData(), notebook.length(), SQLITE_STATIC);

        switch (incidence->type()) {
        case Incidence::TypeEvent:
            type = "Event";
            break;
        case Incidence::TypeTodo:
            type = "Todo";
            break;
        case Incidence::TypeJournal:
            type = "Journal";
            break;
        case Incidence::TypeFreeBusy:
            type = "FreeBusy";
            break;
        case Incidence::TypeUnknown:
            goto error;
        }
        SL3_bind_text(stmt1, index, type.constData(), type.length(), SQLITE_STATIC);   // NOTE

        summary = incidence->summary().toUtf8();
        SL3_bind_text(stmt1, index, summary.constData(), summary.length(), SQLITE_STATIC);

        category = incidence->categoriesStr().toUtf8();
        SL3_bind_text(stmt1, index, category.constData(), category.length(), SQLITE_STATIC);

        if ((incidence->type() == Incidence::TypeEvent) ||
                (incidence->type() == Incidence::TypeJournal)) {
            SL3_bind_date_time(d->mStorage, stmt1, index, incidence->dtStart(), incidence->allDay());

            // set HasDueDate to false
            SL3_bind_int(stmt1, index, 0);

            QDateTime effectiveDtEnd;
            if (incidence->type() == Incidence::TypeEvent) {
                Event::Ptr event = incidence.staticCast<Event>();
                if (event->hasEndDate()) {
                    // Keep this one day addition for backward compatibility reasons
                    // with existing events in database.
                    if (incidence->allDay()) {
                        effectiveDtEnd = event->dtEnd().addDays(1);
                    } else {
                        effectiveDtEnd = event->dtEnd();
                    }
                }
            }
            SL3_bind_date_time(d->mStorage, stmt1, index, effectiveDtEnd, incidence->allDay());
        } else if (incidence->type() == Incidence::TypeTodo) {
            Todo::Ptr todo = incidence.staticCast<Todo>();
            SL3_bind_date_time(d->mStorage, stmt1, index,
                                   todo->hasStartDate() ? todo->dtStart(true) : QDateTime(), todo->allDay());

            SL3_bind_int(stmt1, index, (int) todo->hasDueDate());

            SL3_bind_date_time(d->mStorage, stmt1, index, todo->hasDueDate() ? todo->dtDue(true) : QDateTime(), todo->allDay());
        }

        if (incidence->type() != Incidence::TypeJournal) {
            SL3_bind_int(stmt1, index, incidence->duration().asSeconds()); // NOTE
        } else {
            SL3_bind_int(stmt1, index, 0);
        }

        SL3_bind_int(stmt1, index, incidence->secrecy()); // NOTE

        if (incidence->type() != Incidence::TypeJournal) {
            location = incidence->location().toUtf8();
            SL3_bind_text(stmt1, index, location.constData(), location.length(), SQLITE_STATIC);
        } else {
            SL3_bind_text(stmt1, index, "", 0, SQLITE_STATIC);
        }

        description = incidence->description().toUtf8();
        SL3_bind_text(stmt1, index, description.constData(), description.length(), SQLITE_STATIC);

        SL3_bind_int(stmt1, index, incidence->status()); // NOTE

        if (incidence->type() != Incidence::TypeJournal) {
            if (incidence->hasGeo()) {
                SL3_bind_double(stmt1, index, incidence->geoLatitude());
                SL3_bind_double(stmt1, index, incidence->geoLongitude());
            } else {
                SL3_bind_double(stmt1, index, INVALID_LATLON);
                SL3_bind_double(stmt1, index, INVALID_LATLON);
            }

            SL3_bind_int(stmt1, index, incidence->priority());

            resources = incidence->resources().join(" ").toUtf8();
            SL3_bind_text(stmt1, index, resources.constData(), resources.length(), SQLITE_STATIC);
        } else {
            SL3_bind_double(stmt1, index, INVALID_LATLON);
            SL3_bind_double(stmt1, index, INVALID_LATLON);
            SL3_bind_int(stmt1, index, 0);
            SL3_bind_text(stmt1, index, "", 0, SQLITE_STATIC);
        }

        if (dbop == DBInsert && incidence->created().isNull())
            incidence->setCreated(QDateTime::currentDateTimeUtc());
        secs = d->mStorage->toOriginTime(incidence->created());
        SL3_bind_int64(stmt1, index, secs);

        secs = d->mStorage->toOriginTime(QDateTime::currentDateTimeUtc());
        SL3_bind_int64(stmt1, index, secs);   // datestamp

        secs = d->mStorage->toOriginTime(incidence->lastModified());
        SL3_bind_int64(stmt1, index, secs);

        SL3_bind_int(stmt1, index, incidence->revision());

        comments = incidence->comments().join(" ").toUtf8();
        SL3_bind_text(stmt1, index, comments.constData(), comments.length(), SQLITE_STATIC);

        // Attachments are now stored in a dedicated table.
        SL3_bind_text(stmt1, index, nullptr, 0, SQLITE_STATIC);

        contact = incidence->contacts().join(" ").toUtf8();
        SL3_bind_text(stmt1, index, contact.constData(), contact.length(), SQLITE_STATIC);

        SL3_bind_int(stmt1, index, 0);      //Invitation status removed. Needed? FIXME

        // Never save recurrenceId as FLOATING_DATE, because the time of a
        // floating date is not guaranteed on read and recurrenceId is used
        // for date-time comparisons.
        SL3_bind_date_time(d->mStorage, stmt1, index, incidence->recurrenceId(), false);

        relatedtouid = incidence->relatedTo().toUtf8();
        SL3_bind_text(stmt1, index, relatedtouid.constData(), relatedtouid.length(), SQLITE_STATIC);

        url = incidence->url().toString().toUtf8();
        SL3_bind_text(stmt1, index, url.constData(), url.length(), SQLITE_STATIC);

        uid = incidence->uid().toUtf8();
        SL3_bind_text(stmt1, index, uid.constData(), uid.length(), SQLITE_STATIC);

        if (incidence->type() == Incidence::TypeEvent) {
            Event::Ptr event = incidence.staticCast<Event>();
            SL3_bind_int(stmt1, index, (int)event->transparency());
        } else {
            SL3_bind_int(stmt1, index, 0);
        }

        SL3_bind_int(stmt1, index, (int) incidence->localOnly());

        int percentComplete = 0;
        QDateTime effectiveDtCompleted;
        if (incidence->type() == Incidence::TypeTodo) {
            Todo::Ptr todo = incidence.staticCast<Todo>();
            percentComplete = todo->percentComplete();
            if (todo->isCompleted()) {
                if (!todo->hasCompletedDate()) {
                    // If the todo was created by KOrganizer<2.2 it does not have
                    // a correct completion date. Set one now.
                    todo->setCompleted(QDateTime::currentDateTimeUtc());
                }
                effectiveDtCompleted = todo->completed();
            }
        }
        SL3_bind_int(stmt1, index, percentComplete);
        SL3_bind_date_time(d->mStorage, stmt1, index, effectiveDtCompleted, incidence->allDay());

        colorstr = incidence->color().toUtf8();
        SL3_bind_text(stmt1, index, colorstr.constData(), colorstr.length(), SQLITE_STATIC);

        if (dbop == DBUpdate)
            SL3_bind_int(stmt1, index, rowid);
    }

    SL3_step(stmt1);

    if (dbop == DBInsert)
        rowid = sqlite3_last_insert_rowid(d->mDatabase);

    if (stmt2 && !d->modifyCustomproperties(incidence, rowid, dbop, stmt2, stmt3))
        qCWarning(lcMkcal) << "failed to modify customproperties for incidence" << incidence->uid();

    if (stmt4 && !d->modifyAttendees(incidence, rowid, dbop, stmt4, stmt5))
        qCWarning(lcMkcal) << "failed to modify attendees for incidence" << incidence->uid();

    if (stmt6 && !d->modifyAlarms(incidence, rowid, dbop, stmt6, stmt7))
        qCWarning(lcMkcal) << "failed to modify alarms for incidence" << incidence->uid();

    if (stmt8 && !d->modifyRecursives(incidence, rowid, dbop, stmt8, stmt9))
        qCWarning(lcMkcal) << "failed to modify recursives for incidence" << incidence->uid();

    if (stmt10 && !d->modifyRdates(incidence, rowid, dbop, stmt10, stmt11))
        qCWarning(lcMkcal) << "failed to modify rdates for incidence" << incidence->uid();

    if (delAttachmentStmt && !d->modifyAttachments(incidence, rowid, dbop, delAttachmentStmt, insAttachmentStmt))
        qCWarning(lcMkcal) << "failed to modify attachments for incidence" << incidence->uid();

    return true;

error:
    return false;
}

//@cond PRIVATE
bool SqliteFormat::Private::modifyCustomproperties(Incidence::Ptr incidence, int rowid,
                                                   DBOperation dbop,
                                                   sqlite3_stmt *stmt1, sqlite3_stmt *stmt2)
{
    bool success = true;

    if (dbop == DBUpdate || dbop == DBDelete) {
        // In Update always delete all first then insert all
        // In Delete delete with uid at once
        if (!modifyCustomproperty(rowid, QByteArray(), QString(), QString(), DBDelete, stmt1)) {
            qCWarning(lcMkcal) << "failed to modify customproperty for incidence" << incidence->uid();
            success = false;
        }
    }

    if (success && dbop != DBDelete) {
        QMap<QByteArray, QString> mProperties = incidence->customProperties();
        for (QMap<QByteArray, QString>::ConstIterator it = mProperties.begin(); it != mProperties.end(); ++it) {
            if (!modifyCustomproperty(rowid, it.key(), it.value(),
                                      incidence->nonKDECustomPropertyParameters(it.key()),
                                      (dbop == DBUpdate ? DBInsert : dbop), stmt2)) {
                qCWarning(lcMkcal) << "failed to modify customproperty for incidence" << incidence->uid();
                success = false;
            }
        }
    }

    return success;
}

bool SqliteFormat::Private::modifyCustomproperty(int rowid, const QByteArray &key,
                                                 const QString &value, const QString &parameters,
                                                 DBOperation dbop, sqlite3_stmt *stmt)
{
    int rv = 0;
    int index = 1;
    bool success = false;
    QByteArray valueba;
    QByteArray parametersba;

    if (dbop == DBInsert || dbop == DBDelete)
        SL3_bind_int(stmt, index, rowid);

    if (dbop == DBInsert) {
        SL3_bind_text(stmt, index, key.constData(), key.length(), SQLITE_STATIC);
        valueba = value.toUtf8();
        SL3_bind_text(stmt, index, valueba.constData(), valueba.length(), SQLITE_STATIC);

        parametersba = parameters.toUtf8();
        SL3_bind_text(stmt, index, parametersba.constData(), parametersba.length(), SQLITE_STATIC);
    }

    SL3_step(stmt);
    success = true;

error:
    sqlite3_reset(stmt);

    return success;
}

bool SqliteFormat::Private::modifyRdates(Incidence::Ptr incidence, int rowid, DBOperation dbop,
                                         sqlite3_stmt *stmt1, sqlite3_stmt *stmt2)
{
    bool success = true;

    if (dbop == DBUpdate || dbop == DBDelete) {
        // In Update always delete all first then insert all
        // In Delete delete with uid at once
        if (!modifyRdate(rowid, 0, QDateTime(), false, DBDelete, stmt1)) {
            qCWarning(lcMkcal) << "failed to modify rdates for incidence" << incidence->uid();
            success = false;
        }
    }

    if (success && dbop != DBDelete) {
        int type = SqliteFormat::RDate;
        DateList dateList = incidence->recurrence()->rDates();
        DateList::ConstIterator dt;
        for (dt = dateList.constBegin(); dt != dateList.constEnd(); ++dt) {
            if (!modifyRdate(rowid, type, QDateTime((*dt)), true, (dbop == DBUpdate ? DBInsert : dbop),
                             stmt2)) {
                qCWarning(lcMkcal) << "failed to modify rdates for incidence" << incidence->uid();
                success = false;
            }
        }

        type = SqliteFormat::XDate;
        dateList = incidence->recurrence()->exDates();
        for (dt = dateList.constBegin(); dt != dateList.constEnd(); ++dt) {
            if (!modifyRdate(rowid, type, QDateTime((*dt)), true, (dbop == DBUpdate ? DBInsert : dbop),
                             stmt2)) {
                qCWarning(lcMkcal) << "failed to modify xdates for incidence" << incidence->uid();
                success = false;
            }
        }

        // Both for rDateTimes and exDateTimes, there are possible issues
        // with all day events. KCalendarCore::Recurrence::timesInInterval()
        // is returning repeating events in clock time for all day events,
        // Thus being yyyy-mm-ddT00:00:00 and then "converted" to local
        // zone, for display (meaning being after yyyy-mm-ddT00:00:00+xxxx).
        // When saving, we don't want to store this local zone info, otherwise,
        // the saved date-time won't match when read in another time zone.
        type = SqliteFormat::RDateTime;
        DateTimeList dateTimeList = incidence->recurrence()->rDateTimes();
        DateTimeList::ConstIterator it;
        for (it = dateTimeList.constBegin(); it != dateTimeList.constEnd(); ++it) {
            bool allDay(incidence->allDay() && it->timeSpec() == Qt::LocalTime && it->time() == QTime(0,0));
            if (!modifyRdate(rowid, type, *it, allDay, (dbop == DBUpdate ? DBInsert : dbop), stmt2)) {
                qCWarning(lcMkcal) << "failed to modify rdatetimes for incidence" << incidence->uid();
                success = false;
            }
        }

        type = SqliteFormat::XDateTime;
        dateTimeList = incidence->recurrence()->exDateTimes();
        for (it = dateTimeList.constBegin(); it != dateTimeList.constEnd(); ++it) {
            bool allDay(incidence->allDay() && it->timeSpec() == Qt::LocalTime && it->time() == QTime(0,0));
            if (!modifyRdate(rowid, type, *it, allDay, (dbop == DBUpdate ? DBInsert : dbop), stmt2)) {
                qCWarning(lcMkcal) << "failed to modify xdatetimes for incidence" << incidence->uid();
                success = false;
            }
        }
    }

    return success;
}

bool SqliteFormat::Private::modifyRdate(int rowid, int type, const QDateTime &date,
                                        bool allDay, DBOperation dbop, sqlite3_stmt *stmt)
{
    int rv = 0;
    int index = 1;
    bool success = false;

    if (dbop == DBInsert || dbop == DBDelete)
        SL3_bind_int(stmt, index, rowid);

    if (dbop == DBInsert) {
        SL3_bind_int(stmt, index, type);
        SL3_bind_date_time(mStorage, stmt, index, date, allDay);
    }

    SL3_step(stmt);
    success = true;

error:
    sqlite3_reset(stmt);

    return success;
}

bool SqliteFormat::Private::modifyAlarms(Incidence::Ptr incidence, int rowid, DBOperation dbop,
                                         sqlite3_stmt *stmt1, sqlite3_stmt *stmt2)
{
    bool success = true;

    if (dbop == DBUpdate || dbop == DBDelete) {
        // In Update always delete all first then insert all
        // In Delete delete with uid at once
        if (!modifyAlarm(rowid, Alarm::Ptr(), DBDelete, stmt1)) {
            qCWarning(lcMkcal) << "failed to modify alarm for incidence" << incidence->uid();
            success = false;
        }
    }

    if (success && dbop != DBDelete) {
        const Alarm::List &list = incidence->alarms();
        Alarm::List::ConstIterator it;
        for (it = list.begin(); it != list.end(); ++it) {
            if (!modifyAlarm(rowid, *it, (dbop == DBUpdate ? DBInsert : dbop), stmt2)) {
                qCWarning(lcMkcal) << "failed to modify alarm for incidence" << incidence->uid();
                success = false;
            }
        }
    }

    return success;
}

bool SqliteFormat::Private::modifyAlarm(int rowid, Alarm::Ptr alarm,
                                        DBOperation dbop, sqlite3_stmt *stmt)
{
    int rv = 0;
    int index = 1;
    bool success = false;
    QByteArray description;
    QByteArray relation;
    QByteArray attachment;
    QByteArray addresses;
    QByteArray summary;
    QByteArray properties;

    if (dbop == DBInsert || dbop == DBDelete)
        SL3_bind_int(stmt, index, rowid);

    if (dbop == DBInsert) {
        int action = 0; // default Alarm::Invalid
        Alarm::Type type = alarm->type();
        switch (type) {
        case Alarm::Display:
            action = 1;
            description = alarm->text().toUtf8();
            break;
        case Alarm::Procedure:
            action = 2;
            attachment = alarm->programFile().toUtf8();
            if (!alarm->programArguments().isEmpty())
                description = alarm->programArguments().toUtf8();
            break;
        case Alarm::Email:
            action = 3;
            summary = alarm->mailSubject().toUtf8();
            description = alarm->mailText().toUtf8();
            if (alarm->mailAttachments().size() > 0)
                attachment = alarm->mailAttachments().join(" ").toUtf8();
            if (alarm->mailAddresses().size() > 0) {
                QStringList mailaddresses;
                for (int i = 0; i < alarm->mailAddresses().size(); i++) {
                    mailaddresses << alarm->mailAddresses().at(i).email();
                }
                addresses = mailaddresses.join(" ").toUtf8();
            }
            break;
        case Alarm::Audio:
            action = 4;
            if (!alarm->audioFile().isEmpty())
                attachment = alarm->audioFile().toUtf8();
            break;
        default:
            break;
        }

        SL3_bind_int(stmt, index, action);

        if (alarm->repeatCount()) {
            SL3_bind_int(stmt, index, alarm->repeatCount());
            SL3_bind_int(stmt, index, alarm->snoozeTime().asSeconds());
        } else {
            SL3_bind_int(stmt, index, 0);
            SL3_bind_int(stmt, index, 0);
        }

        if (alarm->hasStartOffset()) {
            SL3_bind_int(stmt, index, alarm->startOffset().asSeconds());
            relation = QString("startTriggerRelation").toUtf8();
            SL3_bind_text(stmt, index, relation.constData(), relation.length(), SQLITE_STATIC);
            SL3_bind_int(stmt, index, 0); // time
            SL3_bind_int(stmt, index, 0); // localtime
            SL3_bind_text(stmt, index, "", 0, SQLITE_STATIC);
        } else if (alarm->hasEndOffset()) {
            SL3_bind_int(stmt, index, alarm->endOffset().asSeconds());
            relation = QString("endTriggerRelation").toUtf8();
            SL3_bind_text(stmt, index, relation.constData(), relation.length(), SQLITE_STATIC);
            SL3_bind_int(stmt, index, 0); // time
            SL3_bind_int(stmt, index, 0); // localtime
            SL3_bind_text(stmt, index, "", 0, SQLITE_STATIC);
        } else {
            SL3_bind_int(stmt, index, 0); // offset
            SL3_bind_text(stmt, index, "", 0, SQLITE_STATIC); // relation
            SL3_bind_date_time(mStorage, stmt, index, alarm->time(), false);
        }

        SL3_bind_text(stmt, index, description.constData(), description.length(), SQLITE_STATIC);
        SL3_bind_text(stmt, index, attachment.constData(), attachment.length(), SQLITE_STATIC);
        SL3_bind_text(stmt, index, summary.constData(), summary.length(), SQLITE_STATIC);
        SL3_bind_text(stmt, index, addresses.constData(), addresses.length(), SQLITE_STATIC);

        QStringList list;
        const QMap<QByteArray, QString> custom = alarm->customProperties();
        for (QMap<QByteArray, QString>::ConstIterator c = custom.begin(); c != custom.end();  ++c) {
            list.append(c.key());
            list.append(c.value());
        }
        if (!list.isEmpty())
            properties = list.join("\r\n").toUtf8();

        SL3_bind_text(stmt, index, properties.constData(), properties.length(), SQLITE_STATIC);
        SL3_bind_int(stmt, index, (int)alarm->enabled());
    }

    SL3_step(stmt);
    success = true;

error:
    sqlite3_reset(stmt);

    return success;
}

bool SqliteFormat::Private::modifyRecursives(Incidence::Ptr incidence, int rowid, DBOperation dbop,
                                             sqlite3_stmt *stmt1, sqlite3_stmt *stmt2)
{
    bool success = true;

    if (dbop == DBUpdate || dbop == DBDelete) {
        // In Update always delete all first then insert all
        // In Delete delete with uid at once
        if (!modifyRecursive(rowid, NULL, DBDelete, stmt1, 1)) {
            qCWarning(lcMkcal) << "failed to modify recursive for incidence" << incidence->uid();
            success = false;
        }
    }

    if (success && dbop != DBDelete) {
        const RecurrenceRule::List &listRR = incidence->recurrence()->rRules();
        RecurrenceRule::List::ConstIterator it;
        for (it = listRR.begin(); it != listRR.end(); ++it) {
            if (!modifyRecursive(rowid, *it, (dbop == DBUpdate ? DBInsert : dbop), stmt2, 1)) {
                qCWarning(lcMkcal) << "failed to modify recursive for incidence" << incidence->uid();
                success = false;
            }
        }
        const RecurrenceRule::List &listER = incidence->recurrence()->exRules();
        for (it = listER.begin(); it != listER.end(); ++it) {
            if (!modifyRecursive(rowid, *it, (dbop == DBUpdate ? DBInsert : dbop), stmt2, 2)) {
                qCWarning(lcMkcal) << "failed to modify recursive for incidence" << incidence->uid();
                success = false;
            }
        }
    }

    return success;
}

bool SqliteFormat::Private::modifyRecursive(int rowid, RecurrenceRule *rule, DBOperation dbop,
                                            sqlite3_stmt *stmt, const int &type)
{
    int rv = 0;
    int index = 1;
    bool success = false;

    QByteArray bySeconds;
    QByteArray byMinutes;
    QByteArray byHours;
    QByteArray byDays;
    QByteArray byDayPoss;
    QByteArray byMonthDays;
    QByteArray byYearDays;
    QByteArray byWeekNumbers;
    QByteArray byMonths;
    QByteArray bySetPos;

    if (dbop == DBInsert || dbop == DBDelete)
        SL3_bind_int(stmt, index, rowid);

    if (dbop == DBInsert) {
        SL3_bind_int(stmt, index, type);

        SL3_bind_int(stmt, index, (int)rule->recurrenceType()); // frequency

        SL3_bind_date_time(mStorage, stmt, index, rule->endDt(), rule->allDay());

        SL3_bind_int(stmt, index, rule->duration());  // count

        SL3_bind_int(stmt, index, (int)rule->frequency()); // interval

        QString number;
        QStringList byL;
        QList<int>::iterator i;
        QList<int> byList;

#define writeSetByList( listname )                  \
    byList = rule->listname();                      \
    byL.clear();                            \
    for (i = byList.begin(); i != byList.end(); ++i) {          \
      number.setNum(*i);                        \
      byL << number;                            \
    }                                   \
    listname = byL.join(" ").toUtf8();                  \
    SL3_bind_text(stmt, index, listname.constData(), listname.length(), SQLITE_STATIC);

        // BYSECOND, MINUTE and HOUR, MONTHDAY, YEARDAY, WEEKNUMBER, MONTH
        // and SETPOS are standard int lists, so we can treat them with the
        // same macro
        writeSetByList(bySeconds);
        writeSetByList(byMinutes);
        writeSetByList(byHours);

        // BYDAY is a special case, since it's not an int list
        QList<RecurrenceRule::WDayPos>::iterator j;
        QList<RecurrenceRule::WDayPos> wdList = rule->byDays();
        byL.clear();
        for (j = wdList.begin(); j != wdList.end(); ++j) {
            number.setNum((*j).day());
            byL << number;
        }
        byDays =  byL.join(" ").toUtf8();
        SL3_bind_text(stmt, index, byDays.constData(), byDays.length(), SQLITE_STATIC);

        byL.clear();
        for (j = wdList.begin(); j != wdList.end(); ++j) {
            number.setNum((*j).pos());
            byL << number;
        }
        byDayPoss =  byL.join(" ").toUtf8();
        SL3_bind_text(stmt, index, byDayPoss.constData(), byDayPoss.length(), SQLITE_STATIC);

        writeSetByList(byMonthDays);
        writeSetByList(byYearDays);
        writeSetByList(byWeekNumbers);
        writeSetByList(byMonths);
        writeSetByList(bySetPos);

#undef writeSetByList

        SL3_bind_int(stmt, index, rule->weekStart());
    }

    SL3_step(stmt);
    success = true;

error:
    sqlite3_reset(stmt);

    return success;
}

bool SqliteFormat::Private::modifyAttendees(Incidence::Ptr incidence, int rowid, DBOperation dbop,
                                            sqlite3_stmt *stmt1, sqlite3_stmt *stmt2)
{
    bool success = true;

    if (dbop == DBUpdate || dbop == DBDelete) {
        // In Update always delete all first then insert all
        // In Delete delete with uid at once
        if (!modifyAttendee(rowid, Attendee(), DBDelete, stmt1, false)) {
            qCWarning(lcMkcal) << "failed to modify attendee for incidence" << incidence->uid();
            success = false;
        }
    }

    if (success && dbop != DBDelete) {
        // FIXME: this doesn't fully save and restore attendees as they were set.
        // e.g. has constraints that every attendee must have email and they need to be unique among the attendees.
        // also this forces attendee list to include the organizer.
        QString organizerEmail;
        if (!incidence->organizer().isEmpty()) {
            organizerEmail = incidence->organizer().email();
            const Attendee organizer(incidence->organizer().name(), organizerEmail);
            if (!modifyAttendee(rowid, organizer,
                                (dbop == DBUpdate ? DBInsert : dbop), stmt2, true)) {
                qCWarning(lcMkcal) << "failed to modify organizer for incidence" << incidence->uid();
                success = false;
            }
        }
        const Attendee::List &list = incidence->attendees();
        Attendee::List::ConstIterator it;
        for (it = list.begin(); it != list.end(); ++it) {
            if (it->email().isEmpty()) {
                qCWarning(lcMkcal) << "Attendee doesn't have an email address";
                continue;
            } else if (it->email() == organizerEmail) {
                continue; // already added above
            }
            if (!modifyAttendee(rowid, *it, (dbop == DBUpdate ? DBInsert : dbop), stmt2, false)) {
                qCWarning(lcMkcal) << "failed to modify attendee for incidence" << incidence->uid();
                success = false;
            }
        }
    }

    return success;
}

bool SqliteFormat::Private::modifyAttendee(int rowid, const Attendee &attendee, DBOperation dbop,
                                           sqlite3_stmt *stmt, bool isOrganizer)
{
    int rv = 0;
    int index = 1;
    bool success = false;
    QByteArray email;
    QByteArray name;
    QByteArray delegate;
    QByteArray delegator;

    if (dbop == DBInsert || dbop == DBDelete)
        SL3_bind_int(stmt, index, rowid);

    if (dbop == DBInsert) {
        email = attendee.email().toUtf8();
        SL3_bind_text(stmt, index, email.constData(), email.length(), SQLITE_STATIC);

        name = attendee.name().toUtf8();
        SL3_bind_text(stmt, index, name.constData(), name.length(), SQLITE_STATIC);

        SL3_bind_int(stmt, index, (int)isOrganizer);

        SL3_bind_int(stmt, index, (int)attendee.role());

        SL3_bind_int(stmt, index, (int)attendee.status());

        SL3_bind_int(stmt, index, (int)attendee.RSVP());

        delegate = attendee.delegate().toUtf8();
        SL3_bind_text(stmt, index, delegate.constData(), delegate.length(), SQLITE_STATIC);

        delegator = attendee.delegator().toUtf8();
        SL3_bind_text(stmt, index, delegator.constData(), delegator.length(), SQLITE_STATIC);
    }

    SL3_step(stmt);
    success = true;

error:
    if (!success) {
        qCWarning(lcMkcal) << "Sqlite error:" << sqlite3_errmsg(mDatabase);
    }
    sqlite3_reset(stmt);

    return success;
}

bool SqliteFormat::Private::modifyAttachments(Incidence::Ptr incidence,
                                              int rowid, DBOperation dbop,
                                              sqlite3_stmt *deleteStatement,
                                              sqlite3_stmt *insertStatement)
{
    bool success = true;

    if (dbop == DBUpdate || dbop == DBDelete) {
        int rv = 0;
        int index = 1;
        // In Update always delete all first then insert all
        // In Delete delete with uid at once
        SL3_bind_int(deleteStatement, index, rowid);
        SL3_step(deleteStatement);
        sqlite3_reset(deleteStatement);
    }

    if (dbop != DBDelete) {
        const Attachment::List &list = incidence->attachments();
        Attachment::List::ConstIterator it;
        for (it = list.begin(); it != list.end(); ++it) {
            int rv = 0;
            int index = 1;

            SL3_bind_int(insertStatement, index, rowid);
            if (it->isBinary()) {
                SL3_bind_blob(insertStatement, index, it->decodedData().constData(), it->size(), SQLITE_STATIC);
                SL3_bind_text(insertStatement, index, nullptr, 0, SQLITE_STATIC);
            } else if (it->isUri()) {
                const QByteArray uri = it->uri().toUtf8();
                SL3_bind_blob(insertStatement, index, nullptr, 0, SQLITE_STATIC);
                SL3_bind_text(insertStatement, index, uri.constData(), uri.length(), SQLITE_STATIC);
            } else {
                continue;
            }
            const QByteArray mime = it->mimeType().toUtf8();
            SL3_bind_text(insertStatement, index, mime.constData(), mime.length(), SQLITE_STATIC);
            SL3_bind_int(insertStatement, index, (it->showInline() ? 1 : 0));
            const QByteArray label = it->label().toUtf8();
            SL3_bind_text(insertStatement, index, label.constData(), label.length(), SQLITE_STATIC);
            SL3_bind_int(insertStatement, index, (it->isLocal() ? 1 : 0));
            SL3_step(insertStatement);
            sqlite3_reset(insertStatement);
        }
    }

    return success;

error:
    if (!success) {
        qCWarning(lcMkcal) << "cannot modify attachment for incidence" << incidence->instanceIdentifier();
        qCWarning(lcMkcal) << "Sqlite error:" << sqlite3_errmsg(mDatabase);
    }
    if (deleteStatement) {
        sqlite3_reset(deleteStatement);
    }
    if (insertStatement) {
        sqlite3_reset(insertStatement);
    }

    return false;
}

bool SqliteFormat::Private::modifyCalendarProperties(Notebook::Ptr notebook, DBOperation dbop)
{
    QByteArray id(notebook->uid().toUtf8());
    // In Update always delete all first then insert all
    if (dbop == DBUpdate && !deleteCalendarProperties(id)) {
        qCWarning(lcMkcal) << "failed to delete calendarproperties for notebook" << id;
        return false;
    }

    bool success = true;
    if (dbop == DBInsert || dbop == DBUpdate) {
        QList<QByteArray> properties = notebook->customPropertyKeys();
        for (QList<QByteArray>::ConstIterator it = properties.constBegin();
             it != properties.constEnd(); ++it) {
            if (!insertCalendarProperty(id, *it, notebook->customProperty(*it).toUtf8())) {
                qCWarning(lcMkcal) << "failed to insert calendarproperty" << *it << "in notebook" << id;
                success = false;
            }
        }
    }
    return success;
}

bool SqliteFormat::Private::deleteCalendarProperties(const QByteArray &id)
{
    int rv = 0;
    int index = 1;
    bool success = false;

    const char *query = DELETE_CALENDARPROPERTIES;
    int qsize = sizeof(DELETE_CALENDARPROPERTIES);
    sqlite3_stmt *stmt = NULL;

    SL3_prepare_v2(mDatabase, query, qsize, &stmt, NULL);
    SL3_bind_text(stmt, index, id.constData(), id.length(), SQLITE_STATIC);
    SL3_step(stmt);
    success = true;

error:
    sqlite3_finalize(stmt);

    return success;
}

bool SqliteFormat::Private::insertCalendarProperty(const QByteArray &id,
                                                   const QByteArray &key,
                                                   const QByteArray &value)
{
    int rv = 0;
    int index = 1;
    bool success = false;

    if (!mInsertCalProps) {
        const char *query = INSERT_CALENDARPROPERTIES;
        int qsize = sizeof(INSERT_CALENDARPROPERTIES);
        SL3_prepare_v2(mDatabase, query, qsize, &mInsertCalProps, NULL);
    }

    SL3_bind_text(mInsertCalProps, index, id.constData(), id.length(), SQLITE_STATIC);
    SL3_bind_text(mInsertCalProps, index, key.constData(), key.length(), SQLITE_STATIC);
    SL3_bind_text(mInsertCalProps, index, value.constData(), value.length(), SQLITE_STATIC);
    SL3_step(mInsertCalProps);
    success = true;

error:
    sqlite3_reset(mInsertCalProps);

    return success;
}
//@endcond

Notebook::Ptr SqliteFormat::selectCalendars(sqlite3_stmt *stmt)
{
    int rv = 0;
    Notebook::Ptr notebook;
    sqlite3_int64 date;
    QDateTime syncDate = QDateTime();
    QDateTime modifiedDate = QDateTime();
    QDateTime creationDate = QDateTime();

    SL3_step(stmt);

    if (rv == SQLITE_ROW) {
        QString id = QString::fromUtf8((const char *)sqlite3_column_text(stmt, 0));
        QString name = QString::fromUtf8((const char *)sqlite3_column_text(stmt, 1));
        QString description = QString::fromUtf8((const char *)sqlite3_column_text(stmt, 2));
        QString color = QString::fromUtf8((const char *)sqlite3_column_text(stmt, 3));
        int flags = (int)sqlite3_column_int(stmt, 4);
        date = sqlite3_column_int64(stmt, 5);
        QString plugin = QString::fromUtf8((const char *)sqlite3_column_text(stmt, 6));
        QString account = QString::fromUtf8((const char *)sqlite3_column_text(stmt, 7));
        int attachmentSize = sqlite3_column_int(stmt, 8);
        syncDate = d->mStorage->fromOriginTime(date);
        date = sqlite3_column_int64(stmt, 9);
        modifiedDate = d->mStorage->fromOriginTime(date);
        QString sharedWith = QString::fromUtf8((const char *)sqlite3_column_text(stmt, 10));
        QString syncProfile = QString::fromUtf8((const char *)sqlite3_column_text(stmt, 11));
        date = sqlite3_column_int64(stmt, 12);
        creationDate = d->mStorage->fromOriginTime(date);

        notebook = Notebook::Ptr(new Notebook(name, description));
        notebook->setUid(id);
        notebook->setColor(color);
        notebook->setFlags(flags);
        notebook->setPluginName(plugin);
        notebook->setAccount(account);
        notebook->setAttachmentSize(attachmentSize);
        notebook->setSyncDate(syncDate);
        notebook->setSharedWithStr(sharedWith);
        notebook->setSyncProfile(syncProfile);
        notebook->setCreationDate(creationDate);

        if (!d->selectCalendarProperties(notebook)) {
            qCWarning(lcMkcal) << "failed to get calendarproperties for notebook" << id;
        }

        // This has to be called last! Otherwise the last modified date
        // will be roughly now, and not whenever notebook was really last
        // modified.
        notebook->setModifiedDate(modifiedDate);
    }

error:
    return notebook;
}

static QDateTime getDateTime(SqliteStorage *storage, sqlite3_stmt *stmt, int index, bool *isDate = 0)
{
    sqlite3_int64 date;
    const QByteArray timezone((const char *)sqlite3_column_text(stmt, index + 2));
    QDateTime dateTime;

    if (timezone.isEmpty()) {
        // consider empty timezone as clock time
        date = sqlite3_column_int64(stmt, index + 1);
        if (date || sqlite3_column_int64(stmt, index)) {
            dateTime = storage->fromOriginTime(date);
            dateTime.setTimeSpec(Qt::LocalTime);
        }
        if (isDate) {
            // This is a workaround, for wrongly stored date
            // as a date and time and not as a floating date.
            QTime localTime(dateTime.time());
            *isDate = dateTime.isValid() &&
                localTime.hour() == 0 &&
                localTime.minute() == 0 &&
                localTime.second() == 0;
        }
    } else if (timezone == QStringLiteral(FLOATING_DATE)) {
        date = sqlite3_column_int64(stmt, index + 1);
        dateTime = storage->fromOriginTime(date);
        dateTime.setTimeSpec(Qt::LocalTime);
        dateTime.setTime(QTime(0, 0, 0));
        if (isDate) {
            *isDate = dateTime.isValid();
        }
    } else {
        date = sqlite3_column_int64(stmt, index);
        dateTime = storage->fromOriginTime(date, timezone);
        if (!dateTime.isValid()) {
            // timezone is specified but invalid?
            // fall back to local seconds from origin as clock time.
            date = sqlite3_column_int64(stmt, index + 1);
            dateTime = storage->fromLocalOriginTime(date);
        }
        if (isDate) {
            *isDate = false;
        }
    }
    return dateTime;
}

Incidence::Ptr SqliteFormat::selectComponents(sqlite3_stmt *stmt1, sqlite3_stmt *stmt2,
                                              sqlite3_stmt *stmt3, sqlite3_stmt *stmt4,
                                              sqlite3_stmt *stmt5, sqlite3_stmt *stmt6,
                                              sqlite3_stmt *attachmentStmt,
                                              QString &notebook)
{
    int rv = 0;
    int index = 0;
    Incidence::Ptr incidence;
    QString type;
    QString timezone;
    int rowid;

    SL3_step(stmt1);

    if (rv == SQLITE_ROW) {

        QByteArray type((const char *)sqlite3_column_text(stmt1, 2));
        if (type == "Event") {
            // Set Event specific data.
            Event::Ptr event = Event::Ptr(new Event());
            event->setAllDay(false);

            bool startIsDate;
            QDateTime start = getDateTime(d->mStorage, stmt1, 5, &startIsDate);
            if (start.isValid()) {
                event->setDtStart(start);
            } else {
                // start date time is mandatory in RFC5545 for VEVENTS.
                event->setDtStart(d->mStorage->fromOriginTime(0));
            }

            bool endIsDate;
            QDateTime end = getDateTime(d->mStorage, stmt1, 9, &endIsDate);
            if (startIsDate && (!end.isValid() || endIsDate)) {
                event->setAllDay(true);
                // Keep backward compatibility with already saved events with end + 1.
                if (end.isValid()) {
                    end = end.addDays(-1);
                    if (end == start) {
                        end = QDateTime();
                    }
                }
            }
            if (end.isValid()) {
                event->setDtEnd(end);
            }
            incidence = event;
        } else if (type == "Todo") {
            // Set Todo specific data.
            Todo::Ptr todo = Todo::Ptr(new Todo());
            todo->setAllDay(false);

            bool startIsDate;
            QDateTime start = getDateTime(d->mStorage, stmt1, 5, &startIsDate);
            if (start.isValid()) {
                todo->setDtStart(start);
            }

            bool hasDueDate(sqlite3_column_int(stmt1, 8));
            bool dueIsDate;
            QDateTime due = getDateTime(d->mStorage, stmt1, 9, &dueIsDate);
            if (due.isValid()) {
                if (start.isValid() && due == start && !hasDueDate) {
                    due = QDateTime();
                } else {
                    todo->setDtDue(due, true);
                }
            }

            if (startIsDate && (!due.isValid() || (dueIsDate && due > start))) {
                todo->setAllDay(true);
            }
            incidence = todo;
        } else if (type == "Journal") {
            // Set Journal specific data.
            Journal::Ptr journal = Journal::Ptr(new Journal());

            bool startIsDate;
            QDateTime start = getDateTime(d->mStorage, stmt1, 5, &startIsDate);
            journal->setDtStart(start);
            journal->setAllDay(startIsDate);
            incidence = journal;
        }

        if (!incidence) {
            return Incidence::Ptr();
        }

        // Set common Incidence data.
        rowid = sqlite3_column_int(stmt1, index++);

        notebook = QString::fromUtf8((const char *)sqlite3_column_text(stmt1, index++));

        index++;

        incidence->setSummary(QString::fromUtf8((const char *)sqlite3_column_text(stmt1, index++)));

        incidence->setCategories(QString::fromUtf8((const char *)sqlite3_column_text(stmt1, index++)));

        index++;
        index++;
        index++;
        index++;
        index++;
        index++;
        index++;

        int duration = sqlite3_column_int(stmt1, index++);
        if (duration != 0) {
            incidence->setDuration(Duration(duration, Duration::Seconds));
        }
        incidence->setSecrecy(
            (Incidence::Secrecy)sqlite3_column_int(stmt1, index++));

        incidence->setLocation(
            QString::fromUtf8((const char *)sqlite3_column_text(stmt1, index++)));

        incidence->setDescription(
            QString::fromUtf8((const char *)sqlite3_column_text(stmt1, index++)));

        incidence->setStatus(
            (Incidence::Status)sqlite3_column_int(stmt1, index++));

        incidence->setGeoLatitude(sqlite3_column_double(stmt1, index++));
        incidence->setGeoLongitude(sqlite3_column_double(stmt1, index++));
        if (incidence->geoLatitude() != INVALID_LATLON) {
            incidence->setHasGeo(true);
        }

        incidence->setPriority(sqlite3_column_int(stmt1, index++));

        QString Resources = QString::fromUtf8((const char *)sqlite3_column_text(stmt1, index++));
        incidence->setResources(Resources.split(' '));

        incidence->setCreated(d->mStorage->fromOriginTime(
                                  sqlite3_column_int64(stmt1, index++)));

        QDateTime dtstamp = d->mStorage->fromOriginTime(sqlite3_column_int64(stmt1, index++));

        incidence->setLastModified(
            d->mStorage->fromOriginTime(sqlite3_column_int64(stmt1, index++)));

        incidence->setRevision(sqlite3_column_int(stmt1, index++));

        QString Comment = QString::fromUtf8((const char *) sqlite3_column_text(stmt1, index++));
        if (!Comment.isEmpty()) {
            QStringList CommL = Comment.split(' ');
            for (QStringList::Iterator it = CommL.begin(); it != CommL.end(); ++it) {
                incidence->addComment(*it);
            }
        }

        // Old way to store attachment, deprecated.
        QString Att = QString::fromUtf8((const char *) sqlite3_column_text(stmt1, index++));

        incidence->addContact(
            QString::fromUtf8((const char *) sqlite3_column_text(stmt1, index++)));

        //Invitation status (removed but still on DB)
        ++index;

        QDateTime rid = getDateTime(d->mStorage, stmt1, index);
        if (rid.isValid()) {
            incidence->setRecurrenceId(rid);
        } else {
            incidence->setRecurrenceId(QDateTime());
        }
        index += 3;

        QString relatedtouid = QString::fromUtf8((const char *) sqlite3_column_text(stmt1, index++));
        incidence->setRelatedTo(relatedtouid);

        QUrl url(QString::fromUtf8((const char *)sqlite3_column_text(stmt1, index++)));
        if (url.isValid()) {
            incidence->setUrl(url);
        }

        // set the real uid to uid
        incidence->setUid(QString::fromUtf8((const char *) sqlite3_column_text(stmt1, index++)));

        if (incidence->type() == Incidence::TypeEvent) {
            Event::Ptr event = incidence.staticCast<Event>();
            int transparency = sqlite3_column_int(stmt1, index);
            event->setTransparency((Event::Transparency) transparency);
        }

        index++;

        incidence->setLocalOnly(sqlite3_column_int(stmt1, index++)); //LocalOnly

        if (incidence->type() == Incidence::TypeTodo) {
            Todo::Ptr todo = incidence.staticCast<Todo>();
            todo->setPercentComplete(sqlite3_column_int(stmt1, index++));
            QDateTime completed = getDateTime(d->mStorage, stmt1, index);
            if (completed.isValid())
                todo->setCompleted(completed);
            index += 3;
        } else {
            index += 4;
        }

        index++; //DateDeleted

        QString colorstr = QString::fromUtf8((const char *) sqlite3_column_text(stmt1, index++));
        if (!colorstr.isEmpty()) {
            incidence->setColor(colorstr);
        }
//    kDebug() << "loaded component for incidence" << incidence->uid() << "notebook" << notebook;

        if (stmt2 && !d->selectCustomproperties(incidence, rowid, stmt2)) {
            qCWarning(lcMkcal) << "failed to get customproperties for incidence" << incidence->uid() << "notebook" << notebook;
        }
        if (stmt3 && !d->selectAttendees(incidence, rowid, stmt3)) {
            qCWarning(lcMkcal) << "failed to get attendees for incidence" << incidence->uid() << "notebook" << notebook;
        }
        if (stmt4 && !d->selectAlarms(incidence, rowid, stmt4)) {
            qCWarning(lcMkcal) << "failed to get alarms for incidence" << incidence->uid() << "notebook" << notebook;
        }
        if (stmt5 && !d->selectRecursives(incidence, rowid, stmt5)) {
            qCWarning(lcMkcal) << "failed to get recursive for incidence" << incidence->uid() << "notebook" << notebook;
        }
        if (stmt6 && !d->selectRdates(incidence, rowid, stmt6)) {
            qCWarning(lcMkcal) << "failed to get rdates for incidence" << incidence->uid() << "notebook" << notebook;
        }
        if (attachmentStmt && !d->selectAttachments(incidence, rowid, attachmentStmt)) {
            qCWarning(lcMkcal) << "failed to get attachments for incidence" << incidence->uid() << "notebook" << notebook;
        }
        // Backward compatibility with the old attachment storage.
        if (!Att.isEmpty() && incidence->attachments().isEmpty()) {
            QStringList AttL = Att.split(' ');
            for (QStringList::Iterator it = AttL.begin(); it != AttL.end(); ++it) {
                incidence->addAttachment(Attachment(*it));
            }
        }
    }

error:
    return incidence;
}

//@cond PRIVATE
int SqliteFormat::Private::selectRowId(Incidence::Ptr incidence)
{
    int rv = 0;
    int index = 1;
    const char *query = NULL;
    int qsize = 0;
    sqlite3_stmt *stmt = NULL;

    QByteArray u;
    qint64 secsRecurId;
    int rowid = 0;

    query = SELECT_ROWID_FROM_COMPONENTS_BY_UID_AND_RECURID;
    qsize = sizeof(SELECT_ROWID_FROM_COMPONENTS_BY_UID_AND_RECURID);

    SL3_prepare_v2(mDatabase, query, qsize, &stmt, NULL);
    u = incidence->uid().toUtf8();
    SL3_bind_text(stmt, index, u.constData(), u.length(), SQLITE_STATIC);
    if (incidence->recurrenceId().isValid()) {
        if (incidence->recurrenceId().timeSpec() == Qt::LocalTime) {
            secsRecurId = mStorage->toLocalOriginTime(incidence->recurrenceId());
        } else {
            secsRecurId = mStorage->toOriginTime(incidence->recurrenceId());
        }
        SL3_bind_int64(stmt, index, secsRecurId);
    } else {
        SL3_bind_int64(stmt, index, 0);
    }

    SL3_step(stmt);

    if (rv == SQLITE_ROW) {
        rowid = sqlite3_column_int(stmt, 0);
    }

error:
    sqlite3_reset(stmt);
    sqlite3_finalize(stmt);

    return rowid;
}

bool SqliteFormat::Private::selectCustomproperties(Incidence::Ptr incidence, int rowid,
                                                   sqlite3_stmt *stmt)
{
    int rv = 0;
    int index = 1;
    QMap<QByteArray, QString> map;

    SL3_bind_int(stmt, index, rowid);

    map.clear();

    do {
        SL3_step(stmt);

        if (rv == SQLITE_ROW) {
            // Set Incidence data customproperties

            QByteArray name = (const char *)sqlite3_column_text(stmt, 1);
            QString value = QString::fromUtf8((const char *)sqlite3_column_text(stmt, 2));
            QString parameters = QString::fromUtf8((const char *)sqlite3_column_text(stmt, 3));
            incidence->setNonKDECustomProperty(name, value, parameters);
        }

    } while (rv != SQLITE_DONE);

    return true;

error:
    return false;
}

bool SqliteFormat::Private::selectRdates(Incidence::Ptr incidence, int rowid, sqlite3_stmt *stmt)
{
    int rv = 0;
    int index = 1;
    QString   timezone;
    QDateTime kdt;

    SL3_bind_int(stmt, index, rowid);

    do {
        SL3_step(stmt);

        if (rv == SQLITE_ROW) {
            // Set Incidence data rdates
            int type = sqlite3_column_int(stmt, 1);
            kdt = getDateTime(mStorage, stmt, 2);
            if (kdt.isValid()) {
                if (type == SqliteFormat::RDate || type == SqliteFormat::XDate) {
                    if (type == SqliteFormat::RDate)
                        incidence->recurrence()->addRDate(kdt.date());
                    else
                        incidence->recurrence()->addExDate(kdt.date());
                } else {
                    if (type == SqliteFormat::RDateTime)
                        incidence->recurrence()->addRDateTime(kdt);
                    else
                        incidence->recurrence()->addExDateTime(kdt);
                }
            }
        }

    } while (rv != SQLITE_DONE);

    return true;

error:
    return false;
}

bool SqliteFormat::Private::selectRecursives(Incidence::Ptr incidence, int rowid,
                                             sqlite3_stmt *stmt)
{
    int  rv = 0;
    int  index = 1;
    QString   timezone;
    QDateTime kdt;
    QDateTime dt;

    SL3_bind_int(stmt, index, rowid);

    do {
        SL3_step(stmt);

        if (rv == SQLITE_ROW) {
            // Set Incidence data from recursive

            // all BY*
            QList<int> byList;
            QList<int> byList2;
            QStringList byL;
            QStringList byL2;
            QString by;
            QString by2;
            RecurrenceRule *recurrule = new RecurrenceRule();

            if (incidence->dtStart().isValid())
                recurrule->setStartDt(incidence->dtStart());
            else {
                if (incidence->type() == Incidence::TypeTodo) {
                    Todo::Ptr todo = incidence.staticCast<Todo>();
                    recurrule->setStartDt(todo->dtDue(true));
                }
            }

            // Generate the RRULE string
            if (sqlite3_column_int(stmt, 1) == 1)   // ruletype
                recurrule->setRRule(QString("RRULE"));
            else
                recurrule->setRRule(QString("EXRULE"));

            switch (sqlite3_column_int(stmt, 2)) {    // frequency
            case 1:
                recurrule->setRecurrenceType(RecurrenceRule::rSecondly);
                break;
            case 2:
                recurrule->setRecurrenceType(RecurrenceRule::rMinutely);
                break;
            case 3:
                recurrule->setRecurrenceType(RecurrenceRule::rHourly);
                break;
            case 4:
                recurrule->setRecurrenceType(RecurrenceRule::rDaily);
                break;
            case 5:
                recurrule->setRecurrenceType(RecurrenceRule::rWeekly);
                break;
            case 6:
                recurrule->setRecurrenceType(RecurrenceRule::rMonthly);
                break;
            case 7:
                recurrule->setRecurrenceType(RecurrenceRule::rYearly);
                break;
            default:
                recurrule->setRecurrenceType(RecurrenceRule::rNone);
            }

            // Duration & End Date
            bool isAllDay;
            QDateTime until = getDateTime(mStorage, stmt, 3, &isAllDay);
            recurrule->setEndDt(until);
            incidence->recurrence()->setAllDay(until.isValid() ? isAllDay : incidence->allDay());

            int duration = sqlite3_column_int(stmt, 6);  // count
            if (duration == 0 && !recurrule->endDt().isValid()) {
                duration = -1; // work around invalid recurrence state: recurring infinitely but having invalid end date
            } else if (duration > 0) {
                // Ensure that no endDt is saved if duration is provided.
                // This guarantees that the operator== returns true for
                // rRule(withDuration) == savedRRule(withDuration)
                recurrule->setEndDt(QDateTime());
            }
            recurrule->setDuration(duration);
            // Frequency
            recurrule->setFrequency(sqlite3_column_int(stmt, 7)); // interval-field


#define readSetByList( field, setfunc )                 \
      by = QString::fromUtf8((const char *)sqlite3_column_text(stmt, field)); \
      if (!by.isEmpty()) {                      \
        byList.clear();                         \
        byL = by.split(' ');                        \
        for ( QStringList::Iterator it = byL.begin(); it != byL.end(); ++it ) \
          byList.append((*it).toInt());                 \
        if ( !byList.isEmpty() )                    \
          recurrule->setfunc(byList);                   \
      }

            // BYSECOND, MINUTE and HOUR, MONTHDAY, YEARDAY, WEEKNUMBER, MONTH
            // and SETPOS are standard int lists, so we can treat them with the
            // same macro
            readSetByList(8, setBySeconds);
            readSetByList(9, setByMinutes);
            readSetByList(10, setByHours);
            readSetByList(13, setByMonthDays);
            readSetByList(14, setByYearDays);
            readSetByList(15, setByWeekNumbers);
            readSetByList(16, setByMonths);
            readSetByList(17, setBySetPos);

#undef readSetByList

            // BYDAY is a special case, since it's not an int list
            QList<RecurrenceRule::WDayPos> wdList;
            RecurrenceRule::WDayPos pos;
            wdList.clear();
            byList.clear();
            by = QString::fromUtf8((const char *)sqlite3_column_text(stmt, 11));
            by2 = QString::fromUtf8((const char *)sqlite3_column_text(stmt, 12));
            if (!by.isEmpty()) {
                byL = by.split(' ');
                if (!by2.isEmpty())
                    byL2 = by2.split(' ');
                for (int i = 0; i < byL.size(); ++i) {
                    if (!by2.isEmpty()) {
                        pos.setDay(byL.at(i).toInt());
                        pos.setPos(byL2.at(i).toInt());
                        wdList.append(pos);
                    } else {
                        pos.setDay(byL.at(i).toInt());
                        wdList.append(pos);
                    }
                }
                if (!wdList.isEmpty())
                    recurrule->setByDays(wdList);
            }

            // Week start setting
            recurrule->setWeekStart(sqlite3_column_int(stmt, 18));

            if (recurrule->rrule() == "RRULE")
                incidence->recurrence()->addRRule(recurrule);
            else
                incidence->recurrence()->addExRule(recurrule);
        }

    } while (rv != SQLITE_DONE);

    return true;

error:
    return false;
}

bool SqliteFormat::Private::selectAlarms(Incidence::Ptr incidence, int rowid, sqlite3_stmt *stmt)
{
    int rv = 0;
    int index = 1;
    int offset;
    QString   timezone;
    QDateTime kdt;
    QDateTime dt;

    SL3_bind_int(stmt, index, rowid);

    do {
        SL3_step(stmt);

        if (rv == SQLITE_ROW) {
            // Set Incidence data from alarm

            Alarm::Ptr ialarm = incidence->newAlarm();

            // Determine the alarm's action type
            int action = sqlite3_column_int(stmt, 1);
            Alarm::Type type = Alarm::Invalid;

            switch (action) {
            case 1: //ICAL_ACTION_DISPLAY
                type = Alarm::Display;
                break;
            case 2: //ICAL_ACTION_PROCEDURE
                type = Alarm::Procedure;
                break;
            case 3: //ICAL_ACTION_EMAIL
                type = Alarm::Email;
                break;
            case 4: //ICAL_ACTION_AUDIO
                type = Alarm::Audio;
                break;
            default:
                break;
            }

            ialarm->setType(type);

            if (sqlite3_column_int(stmt, 2) > 0)
                ialarm->setRepeatCount(sqlite3_column_int(stmt, 2));
            if (sqlite3_column_int(stmt, 3) > 0)
                ialarm->setSnoozeTime(Duration(sqlite3_column_int(stmt, 3), Duration::Seconds));

            offset = sqlite3_column_int(stmt, 4);
            QString relation = QString::fromUtf8((const char *)sqlite3_column_text(stmt, 5));

            kdt = getDateTime(mStorage, stmt, 6);
            if (kdt.isValid())
                ialarm->setTime(kdt);

            if (!ialarm->hasTime()) {
                if (relation.contains("startTriggerRelation")) {
                    ialarm->setStartOffset(Duration(offset, Duration::Seconds));
                } else if (relation.contains("endTriggerRelation")) {
                    ialarm->setEndOffset(Duration(offset, Duration::Seconds));
                }
            }

            QString description =  QString::fromUtf8((const char *)sqlite3_column_text(stmt, 9));
            QString attachments =  QString::fromUtf8((const char *)sqlite3_column_text(stmt, 10));
            QString summary = QString::fromUtf8((const char *)sqlite3_column_text(stmt, 11));
            QString addresses = QString::fromUtf8((const char *)sqlite3_column_text(stmt, 12));

            switch (ialarm->type()) {
            case Alarm::Display:
                ialarm->setText(description);
                break;
            case Alarm::Procedure:
                ialarm->setProgramFile(attachments);
                ialarm->setProgramArguments(description);
                break;
            case Alarm::Email:
                ialarm->setMailSubject(summary);
                ialarm->setMailText(description);
                if (!attachments.isEmpty())
                    ialarm->setMailAttachments(attachments.split(','));
                if (!addresses.isEmpty()) {
                    Person::List persons;
                    QStringList emails = addresses.split(',');
                    for (int i = 0; i < emails.size(); i++) {
                        persons.append(Person(QString(), emails.at(i)));
                    }
                    ialarm->setMailAddresses(persons);
                }
                break;
            case Alarm::Audio:
                ialarm->setAudioFile(attachments);
                break;
            default:
                break;
            }

            QString properties = QString::fromUtf8((const char *)sqlite3_column_text(stmt, 13));
            if (!properties.isEmpty()) {
                QMap<QByteArray, QString> customProperties;
                QStringList list = properties.split("\r\n");
                for (int i = 0; i < list.size(); i += 2) {
                    QByteArray key;
                    QString value;
                    key = list.at(i).toUtf8();
                    if ((i + 1) < list.size()) {
                        value = list.at(i + 1);
                        customProperties[key] = value;
                    }
                }
                ialarm->setCustomProperties(customProperties);
                QString locationRadius = ialarm->nonKDECustomProperty("X-LOCATION-RADIUS");
                if (!locationRadius.isEmpty()) {
                    ialarm->setLocationRadius(locationRadius.toInt());
                    ialarm->setHasLocationRadius(true);
                }
            }

            ialarm->setEnabled((bool)sqlite3_column_int(stmt, 14));
        }

    } while (rv != SQLITE_DONE);

    return true;

error:
    return false;
}

bool SqliteFormat::Private::selectAttendees(Incidence::Ptr incidence, int rowid,
                                            sqlite3_stmt *stmt)
{
    int rv = 0;
    int index = 1;

    SL3_bind_int(stmt, index, rowid);

    do {
        SL3_step(stmt);

        if (rv == SQLITE_ROW) {
            QString email = QString::fromUtf8((const char *)sqlite3_column_text(stmt, 1));
            QString name = QString::fromUtf8((const char *)sqlite3_column_text(stmt, 2));
            bool isOrganizer = (bool)sqlite3_column_int(stmt, 3);
            Attendee::Role role = (Attendee::Role)sqlite3_column_int(stmt, 4);
            Attendee::PartStat status = (Attendee::PartStat)sqlite3_column_int(stmt, 5);
            bool rsvp = (bool)sqlite3_column_int(stmt, 6);
            if (isOrganizer) {
                incidence->setOrganizer(Person(name, email));
            }
            Attendee attendee(name, email, rsvp, status, role);
            attendee.setDelegate(QString::fromUtf8((const char *)sqlite3_column_text(stmt, 7)));
            attendee.setDelegator(QString::fromUtf8((const char *)sqlite3_column_text(stmt, 8)));
            incidence->addAttendee(attendee, false);
        }
    } while (rv != SQLITE_DONE);

    return true;

error:
    return false;
}

bool SqliteFormat::Private::selectAttachments(Incidence::Ptr incidence, int rowid, sqlite3_stmt *stmt)
{
    int rv = 0;
    int index = 1;

    SL3_bind_int(stmt, index, rowid);

    do {
        SL3_step(stmt);

        if (rv == SQLITE_ROW) {
            Attachment attach;

            QByteArray data = QByteArray((const char *)sqlite3_column_blob(stmt, 1),
                                         sqlite3_column_bytes(stmt, 1));
            if (!data.isEmpty()) {
                attach.setDecodedData(data);
            } else {
                QString uri = QString::fromUtf8((const char *)sqlite3_column_text(stmt, 2));
                if (!uri.isEmpty()) {
                    attach.setUri(uri);
                }
            }
            if (!attach.isEmpty()) {
                attach.setMimeType(QString::fromUtf8((const char *)sqlite3_column_text(stmt, 3)));
                attach.setShowInline(sqlite3_column_int(stmt, 4) != 0);
                attach.setLabel(QString::fromUtf8((const char *)sqlite3_column_text(stmt, 5)));
                attach.setLocal(sqlite3_column_int(stmt, 6) != 0);
                incidence->addAttachment(attach);
            } else {
                qCWarning(lcMkcal) << "Empty attachment for incidence" << incidence->instanceIdentifier();
            }
        }
    } while (rv != SQLITE_DONE);

    return true;

error:
    return false;
}

Person::List SqliteFormat::selectContacts(sqlite3_stmt *stmt)
{
    int rv = 0;
    Person::List list;
    QHash<QString, Person> hash;

    do {
        SL3_step(stmt);

        if (rv == SQLITE_ROW) {
            QString name = QString::fromUtf8((const char *)sqlite3_column_text(stmt, 1));
            QString email = QString::fromUtf8((const char *)sqlite3_column_text(stmt, 0));

            hash.insert(email, Person(name, email));
        }
    } while (rv != SQLITE_DONE);

    list = hash.values().toVector();

error:
    return list;
}

bool SqliteFormat::Private::selectCalendarProperties(Notebook::Ptr notebook)
{
    int rv = 0;
    int index = 1;
    const QByteArray id(notebook->uid().toUtf8());
    bool success = false;

    if (!mSelectCalProps) {
        const char *query = SELECT_CALENDARPROPERTIES_BY_ID;
        int qsize = sizeof(SELECT_CALENDARPROPERTIES_BY_ID);
        SL3_prepare_v2(mDatabase, query, qsize, &mSelectCalProps, NULL);
    }

    SL3_bind_text(mSelectCalProps, index, id.constData(), id.length(), SQLITE_STATIC);
    do {
        SL3_step(mSelectCalProps);
        if (rv == SQLITE_ROW) {
            const QByteArray name = (const char *)sqlite3_column_text(mSelectCalProps, 1);
            const QString value = QString::fromUtf8((const char *)sqlite3_column_text(mSelectCalProps, 2));
            notebook->setCustomProperty(name, value);
        }
    } while (rv != SQLITE_DONE);
    success = true;

error:
    sqlite3_reset(mSelectCalProps);

    return success;
}
