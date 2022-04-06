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

#include "threadedstorage.h"
#include "sqlitestorage.h"

#include <QThread>

using namespace mKCal;

/**
  Private class that helps to provide binary compatibility between releases.
  @internal
*/
//@cond PRIVATE
class mKCal::ThreadedStorage::Private
    : public QObject
    , public StorageBackend::Manager
    , public StorageBackend::Observer
{
    Q_OBJECT
public:
    Private(ThreadedStorage *parent, const QTimeZone &timeZone)
        : QObject(parent)
        , mWorker(new SqliteStorage(timeZone, validateNotebooks))
    {
        mWorker->registerManager(this);
        mWorker->registerObserver(this);
        mWorker->moveToThread(&mWorkerThread);
        connect(&mWorkerThread, &QThread::finished, mWorker, &QObject::deleteLater);
        mWorkerThread.start();
    }

    ~Private()
    {
        mWorker->unregisterObserver(this);
        mWorker->unregisterManager(this);
    }

    // Ownership is transfered to the main thread.
    void newNotebooks(StorageBackend *storage,
                       const StorageBackend::Library &notebooks) override
    {
    }
    void newIncidences(StorageBackend *storage,
                       const StorageBackend::Collection &incidences) override
    {
    }

    // These observer methods will run in the worker thread.
    void storageOpened(StorageBackend *storage,
                       const StorageBackend::Library &notebooks) override
    {
        emit opened(notebooks);
    }
    void storageClosed(StorageBackend *storage) override
    {
        emit closed();
    }
    void storageModified(StorageBackend *storage,
                         const StorageBackend::Library &notebooks) override
    {
        emit modified(notebooks);
    }
    void storageUpdated(StorageBackend *storage,
                        const StorageBackend::Collection &additions,
                        const StorageBackend::Collection &modifications,
                        const StorageBackend::Collection &deletions) override
    {
        emit updated(additions, modifications, deletions);
    }
    void incidenceLoaded(StorageBackend *storage,
                         const StorageBackend::Collection &incidences) override
    {
        emit loaded(incidences);
    }

signals:
    void opened(const StorageBackend::Library &notebooks);
    void closed();
    void modified(const StorageBackend::Library &notebooks);
    void updated(const StorageBackend::Collection &added,
                 const StorageBackend::Collection &modified,
                 const StorageBackend::Collection &deleted);
    void loaded(const StorageBackend::Collection &incidences);

public:
    QThread mWorkerThread;
    SqliteStorage *mWorker;
};
//@endcond

ThreadedStorage::ThreadedStorage(const QTimeZone &timeZone)
    : StorageBackend(timeZone)
    , d(new ThreadedStorage::Private(this, timeZone))
{
    connect(d, &ThreadedStorage::Private::opened,
            this, &StorageBackend::setOpened, Qt::BlockingQueuedConnection);
    connect(d, &ThreadedStorage::Private::closed, this, &StorageBackend::setClosed);
    connect(d, &ThreadedStorage::Private::modified,
            this, &StorageBackend::setModified, Qt::BlockingQueuedConnection);
    connect(d, &ThreadedStorage::Private::updated,
            this, &StorageBackend::setUpdated, Qt::BlockingQueuedConnection);
    connect(d, &ThreadedStorage::Private::loaded,
            this, &StorageBackend::setLoaded, Qt::BlockingQueuedConnection);
}

ThreadedStorage::~ThreadedStorage()
{
}

bool ThreadedStorage::open()
{
    QMetaObject::invokeMethod(d->mWorker, "open", Qt::QueuedConnection);
    return true;
}

bool ThreadedStorage::close()
{
    QMetaObject::invokeMethod(d->mWorker, "close", Qt::QueuedConnection);
    return true;
}

bool ThreadedStorage::load()
{
    QMetaObject::invokeMethod(d->mWorker, "load", Qt::QueuedConnection);
    return true;
}

bool ThreadedStorage::load(const QString &uid, const QDateTime &recurrenceId)
{
    QMetaObject::invokeMethod(d->mWorker, "load", Qt::QueuedConnection,
                              Q_ARG(QString, uid),
                              Q_ARG(QDateTime, recurrenceId));
    return true;
}

bool ThreadedStorage::load(const QDate &date)
{
    QMetaObject::invokeMethod(d->mWorker, "load", Qt::QueuedConnection,
                              Q_ARG(QDate, date));
    return true;
}

bool ThreadedStorage::load(const QDate &start, const QDate &end)
{
    QMetaObject::invokeMethod(d->mWorker, "load", Qt::QueuedConnection,
                              Q_ARG(QDate, start),
                              Q_ARG(QDate, end));
    return true;
}

bool ThreadedStorage::loadSeries(const QString &uid)
{
    QMetaObject::invokeMethod(d->mWorker, "loadSeries", Qt::QueuedConnection,
                              Q_ARG(QString, uid));
    return true;
}

bool ThreadedStorage::loadIncidenceInstance(const QString &instanceIdentifier)
{
    QMetaObject::invokeMethod(d->mWorker, "loadIncidenceInstance", Qt::QueuedConnection,
                              Q_ARG(QString, instanceIdentifier));
    return true;
}

bool ThreadedStorage::loadNotebookIncidences(const QString &notebookUid)
{
    QMetaObject::invokeMethod(d->mWorker, "loadNotebookIncidences", Qt::QueuedConnection,
                              Q_ARG(QString, notebookUid));
    return true;
}

bool ThreadedStorage::loadJournals()
{
    QMetaObject::invokeMethod(d->mWorker, "loadJournals", Qt::QueuedConnection);
    return true;
}

bool ThreadedStorage::loadPlainIncidences()
{
    QMetaObject::invokeMethod(d->mWorker, "loadPlainIncidences", Qt::QueuedConnection);
    return true;
}

bool ThreadedStorage::loadRecurringIncidences()
{
    QMetaObject::invokeMethod(d->mWorker, "loadRecurringIncidences", Qt::QueuedConnection);
    return true;
}

bool ThreadedStorage::loadGeoIncidences()
{
    QMetaObject::invokeMethod(d->mWorker, "loadGeoIncidences", Qt::QueuedConnection);
    return true;
}

bool ThreadedStorage::loadGeoIncidences(float geoLatitude, float geoLongitude,
                                        float diffLatitude, float diffLongitude)
{
    QMetaObject::invokeMethod(d->mWorker, "loadGeoIncidences", Qt::QueuedConnection,
                              Q_ARG(float, geoLatitude),
                              Q_ARG(float, geoLongitude),
                              Q_ARG(float, diffLatitude),
                              Q_ARG(float, diffLongitude));
    return true;
}

bool ThreadedStorage::loadAttendeeIncidences()
{
    QMetaObject::invokeMethod(d->mWorker, "loadAttendeeIncidences", Qt::QueuedConnection);
    return true;
}

int ThreadedStorage::loadUncompletedTodos()
{
    QMetaObject::invokeMethod(d->mWorker, "loadUncompletedTodos", Qt::QueuedConnection);
    return 0;
}

int ThreadedStorage::loadCompletedTodos(bool hasDate, int limit, QDateTime *last)
{
    QMetaObject::invokeMethod(d->mWorker, "loadCompletedTodos", Qt::QueuedConnection,
                              Q_ARG(bool, hasDate),
                              Q_ARG(int, limit),
                              Q_ARG(QDateTime*, nullptr));
    return 0;
}

int ThreadedStorage::loadIncidences(bool hasDate, int limit, QDateTime *last)
{
    QMetaObject::invokeMethod(d->mWorker, "loadIncidences", Qt::QueuedConnection,
                              Q_ARG(bool, hasDate),
                              Q_ARG(int, limit),
                              Q_ARG(QDateTime*, nullptr));
    return 0;
}

int ThreadedStorage::loadFutureIncidences(int limit, QDateTime *last)
{
    QMetaObject::invokeMethod(d->mWorker, "loadFutureIncidences", Qt::QueuedConnection,
                              Q_ARG(int, limit),
                              Q_ARG(QDateTime*, nullptr));
    return 0;
}

int ThreadedStorage::loadGeoIncidences(bool hasDate, int limit, QDateTime *last)
{
    QMetaObject::invokeMethod(d->mWorker, "loadGeoIncidences", Qt::QueuedConnection,
                              Q_ARG(bool, hasDate),
                              Q_ARG(int, limit),
                              Q_ARG(QDateTime*, nullptr));
    return 0;
}

int ThreadedStorage::loadUnreadInvitationIncidences()
{
    QMetaObject::invokeMethod(d->mWorker, "loadUnreadInvitationIncidences", Qt::QueuedConnection);
    return 0;
}

int ThreadedStorage::loadOldInvitationIncidences(int limit, QDateTime *last)
{
    QMetaObject::invokeMethod(d->mWorker, "loadOldInvitationIncidences", Qt::QueuedConnection,
                              Q_ARG(int, limit),
                              Q_ARG(QDateTime*, nullptr));
    return 0;
}

KCalendarCore::Person::List ThreadedStorage::loadContacts()
{
    return {};
}

int ThreadedStorage::loadContactIncidences(const KCalendarCore::Person &person,
                                           int limit, QDateTime *last)
{
    QMetaObject::invokeMethod(d->mWorker, "loadContactIncidences", Qt::QueuedConnection,
                              Q_ARG(KCalendarCore::Person, person),
                              Q_ARG(QString, uid),
                              Q_ARG(QDateTime, recurrenceId));
    return 0;
}

int ThreadedStorage::loadJournals(int limit, QDateTime *last)
{
    QMetaObject::invokeMethod(d->mWorker, "loadJournals", Qt::QueuedConnection,
                              Q_ARG(int, limit),
                              Q_ARG(QDateTime*, nullptr));
    return 0;
}

bool ThreadedStorage::purgeDeletedIncidences(const StorageBackend::Collection &list)
{
    StorageBackend::Collection cpCollection;
    for (StorageBackend::Collection::ConstIterator it = list.constBegin();
         it != list.constEnd(); it++) {
        cpCollection.insert(it.key(), it.value()->clone());
    }
    // Clones will be deleted by the worker thread after use in the
    // storageUpdated() callback.
    QMetaObject::invokeMethod(d->mWorker, "purgeDeletedIncidences", Qt::QueuedConnection,
                              Q_ARG(StorageBackend::Collection, cpCollection));
}

bool ThreadedStorage::storeIncidences(const StorageBackend::SharedCollection &additions,
                                      const StorageBackend::SharedCollection &modifications,
                                      const StorageBackend::SharedCollection &deletions,
                                      StorageBackend::DeleteAction deleteAction)
{
    StorageBackend::Collection cpAdditions;
    for (StorageBackend::SharedCollection::ConstIterator it = additions.constBegin();
         it != added.constEnd(); it++) {
        cpAdditions.insert(it.key(), it.value()->clone());
    }
    StorageBackend::Collection cpModifications;
    for (StorageBackend::SharedCollection::ConstIterator it = modifications.constBegin();
         it != added.constEnd(); it++) {
        cpModifications.insert(it.key(), it.value()->clone());
    }
    StorageBackend::Collection cpDeletions;
    for (StorageBackend::SharedCollection::ConstIterator it = deletions.constBegin();
         it != added.constEnd(); it++) {
        cpDeletions.insert(it.key(), it.value()->clone());
    }
    // Clones will be deleted by the worker thread after use.
    QMetaObject::invokeMethod(d->mWorker, "storeIncidences", Qt::QueuedConnection,
                              Q_ARG(StorageBackend::Collection, cpAdditions),
                              Q_ARG(StorageBackend::Collection, cpModifications),
                              Q_ARG(StorageBackend::Collection, cpDeletions));
}

bool ThreadedStorage::insertedIncidences(KCalendarCore::Incidence::List *list,
                                         const QDateTime &after,
                                         const QString &notebookUid)
{
    return false;
}

bool modifiedIncidences(KCalendarCore::Incidence::List *list,
                        const QDateTime &after,
                        const QString &notebookUid)
{
    return false;
}

bool deletedIncidences(KCalendarCore::Incidence::List *list,
                       const QDateTime &after,
                       const QString &notebookUid);
{
    return false;
}

bool allIncidences(KCalendarCore::Incidence::List *list,
                   const QString &notebookUid)
{
    return false;
}

bool ThreadedStorage::addNotebook(const Notebook &nb)
{
    QMetaObject::invokeMethod(d->mWorker, "addNotebook", Qt::QueuedConnection,
                              Q_ARG(Notebook, nb));
    return true;
}

bool ThreadedStorage::updateNotebook(const Notebook::Ptr &nb)
{
    QMetaObject::invokeMethod(d->mWorker, "updateNotebook", Qt::QueuedConnection,
                              Q_ARG(Notebook, nb));
    return true;
}

bool ThreadedStorage::deleteNotebook(const Notebook::Ptr &nb)
{
    QMetaObject::invokeMethod(d->mWorker, "deleteNotebook", Qt::QueuedConnection,
                              Q_ARG(Notebook, nb));
    return true;
}
