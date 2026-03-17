#ifndef STATUSCHECKER_H
#define STATUSCHECKER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QHash>
#include <QDateTime>
#include <QFutureWatcher>
#include <QMutex>
#include <QAtomicInt>
#include <QDir>

class StatusChecker : public QObject
{
    Q_OBJECT

public:
    static StatusChecker *getInstance();

    // Check status of a single path
    QString checkStatus(const QString &path, bool recurse = true, bool invalidate = true, bool summary = true);

    // Generate menu conditions for multiple paths
    QString generateMenuConditions(const QStringList &paths);

    // Get icon name for status
    QString getStatusIconName(const QString &status);

private slots:
    void onDirectoryCacheReady();

private:
    explicit StatusChecker(QObject *parent = nullptr);
    ~StatusChecker() override = default;

    // Direct status check using git/svn commands (avoids D-Bus and Python 2)
    QString checkStatusDirectly(const QString &path);

    // Batch query all files in a directory (solution 1) - now async
    void cacheDirectoryStatus(const QString &dirPath);

    // Async cache worker
    struct CacheResult {
        QString dirPath;
        QHash<QString, QString> files;
    };
    CacheResult cacheDirectoryStatusSync(const QString &dirPath);

    // Parse git status output
    QString parseGitStatusLine(const QString &line, const QString &filePath);

    // Parse svn status output
    QString parseSvnStatusLine(const QString &line, const QString &filePath);

    // Check if file is ignored by VCS
    bool isFileIgnored(const QString &path, const QString &vcsType, const QDir &vcsDir);

    // Status to icon mapping
    QHash<QString, QString> statusEmblemsHash;

    // Directory-level cache (path -> status)
    QHash<QString, QHash<QString, QString>> dirCache;

    // Track when each directory was last cached
    QHash<QString, QDateTime> dirCacheTime;

    // Async cache watcher
    QFutureWatcher<CacheResult> *cacheWatcher;

    // Current caching task ID (for cancellation)
    QAtomicInt currentTaskId;

    // Mutex for thread-safe cache access
    QMutex cacheMutex;

    // Cache expiration time (5 seconds)
    static const int CACHE_EXPIRE_MS = 5000;
};

#endif // STATUSCHECKER_H
