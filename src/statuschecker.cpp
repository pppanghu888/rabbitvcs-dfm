#include "statuschecker.h"

#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusConnection>
#include <QProcess>
#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <QRegExp>
#include <QCoreApplication>
#include <QtConcurrent>
#include <QMutexLocker>

#define SERVICE "org.google.code.rabbitvcs.RabbitVCS.Checker"
#define OBJECT_PATH "/org/google/code/rabbitvcs/StatusChecker"
#define INTERFACE "org.google.code.rabbitvcs.StatusChecker"

#define ADD_STATUS_EMBLEM(A, B) statusEmblemsHash.insert(#A, "emblem-rabbitvcs-"#B)

static StatusChecker *g_instance = nullptr;

StatusChecker *StatusChecker::getInstance()
{
    if (!g_instance) {
        g_instance = new StatusChecker;
    }
    return g_instance;
}

StatusChecker::StatusChecker(QObject *parent)
    : QObject(parent)
    , currentTaskId(0)
{
    // Initialize status to icon name mapping
    ADD_STATUS_EMBLEM(normal, normal);
    ADD_STATUS_EMBLEM(modified, modified);
    ADD_STATUS_EMBLEM(added, added);
    ADD_STATUS_EMBLEM(deleted, deleted);
    ADD_STATUS_EMBLEM(read-only, locked);
    ADD_STATUS_EMBLEM(locked, locked);
    // unknown - no icon (don't show emblem)
    ADD_STATUS_EMBLEM(ignored, ignored);
    ADD_STATUS_EMBLEM(missing, complicated);
    ADD_STATUS_EMBLEM(replaced, modified);
    ADD_STATUS_EMBLEM(complicated, complicated);
    ADD_STATUS_EMBLEM(calculating, calculating);
    ADD_STATUS_EMBLEM(error, eerror);
    ADD_STATUS_EMBLEM(unversioned, unversioned);
    ADD_STATUS_EMBLEM(conflicted, conflicted);

    // Setup async cache watcher
    cacheWatcher = new QFutureWatcher<CacheResult>(this);
    connect(cacheWatcher, &QFutureWatcher<CacheResult>::finished,
            this, &StatusChecker::onDirectoryCacheReady);

    qDebug() << "StatusChecker initialized with cache expiry:" << CACHE_EXPIRE_MS << "ms";
}

QString StatusChecker::checkStatus(const QString &path, bool recurse, bool invalidate, bool summary)
{
    Q_UNUSED(recurse);
    Q_UNUSED(invalidate);
    Q_UNUSED(summary);

    // Check if directory is already cached
    QFileInfo fileInfo(path);
    QString dirPath = fileInfo.isDir() ? path : fileInfo.path();

    // Check directory cache (with mutex for thread safety)
    {
        QMutexLocker locker(&cacheMutex);
        if (dirCache.contains(dirPath)) {
            QDateTime cacheTime = dirCacheTime[dirPath];
            if (cacheTime.msecsTo(QDateTime::currentDateTime()) < CACHE_EXPIRE_MS) {
                // Directory cache is still valid
                if (dirCache[dirPath].contains(path)) {
                    QString status = dirCache[dirPath][path];
                    //qDebug() << "[DirCache HIT] File:" << path << "Status:" << status;
                    return status;
                }
            } else {
                // Cache expired, remove it
                qDebug() << "[Cache EXPIRED] For directory:" << dirPath;
                dirCache.remove(dirPath);
                dirCacheTime.remove(dirPath);
            }
        }
    }

    // Cache miss - use Solution 3: Direct git/svn call for single file (fast path)
    QString status = checkStatusDirectly(path);

    // Trigger async directory cache for future files in this directory
    // Cancel previous task if still running
    if (cacheWatcher->isRunning()) {
        cacheWatcher->cancel();
        cacheWatcher->waitForFinished();
    }

    // Start new async cache task
    currentTaskId.fetchAndAddRelaxed(1);
    QFuture<CacheResult> future = QtConcurrent::run(
        this, &StatusChecker::cacheDirectoryStatusSync, dirPath);
    cacheWatcher->setFuture(future);

    //qDebug() << "[Direct+Async] File:" << path << "Status:" << status;
    return status;
}

QString StatusChecker::generateMenuConditions(const QStringList &paths)
{
    qDebug() << "=== generateMenuConditions called ===";
    qDebug() << "  paths:" << paths;

    if (paths.isEmpty()) {
        qDebug() << "  No paths provided, returning empty";
        return QString();
    }

    // Generate menu conditions using git/svn commands directly
    // This avoids the GTK compatibility issues with RabbitVCS's GenerateMenuConditions

    bool hasGit = false;
    bool hasSvn = false;
    bool isWorkingCopy = false;
    bool isVersioned = false;
    bool isModified = false;
    bool isAdded = false;
    bool isDeleted = false;
    bool isDir = false;
    bool exists = false;
    bool isInAOrAWorkingCopy = false;
    bool isFile = false;

    for (const QString &path : paths) {
        QFileInfo fileInfo(path);
        if (!fileInfo.exists()) {
            continue;
        }

        exists = true;
        isDir = fileInfo.isDir();
        isFile = fileInfo.isFile();

        QDir dir(fileInfo.isDir() ? path : fileInfo.path());

        // Check Git
        QDir gitDir(dir);
        while (gitDir.exists() && !gitDir.exists(".git")) {
            if (!gitDir.cdUp()) break;
        }

        if (gitDir.exists(".git")) {
            hasGit = true;
            isWorkingCopy = true;
            isInAOrAWorkingCopy = true;
            isVersioned = true;

            // Check file status using git
            QProcess process;
            process.setWorkingDirectory(gitDir.absolutePath());
            process.start("git", QStringList() << "status" << "--porcelain" << path);
            process.waitForFinished(3000);
            QString output = process.readAllStandardOutput().trimmed();

            if (!output.isEmpty()) {
                QChar firstChar = output.at(0);
                if (firstChar == 'M' || firstChar == 'm') isModified = true;
                else if (firstChar == 'A' || firstChar == 'a') isAdded = true;
                else if (firstChar == 'D' || firstChar == 'd') isDeleted = true;
                else if (firstChar == 'R' || firstChar == 'r') isModified = true;
                else if (firstChar == 'C' || firstChar == 'c') isModified = true;
            }
            continue;
        }

        // Check SVN
        QDir svnDir(dir);
        while (svnDir.exists() && !svnDir.exists(".svn")) {
            if (!svnDir.cdUp()) break;
        }

        if (svnDir.exists(".svn")) {
            hasSvn = true;
            isWorkingCopy = true;
            isInAOrAWorkingCopy = true;
            isVersioned = true;

            // Check file status using svn
            QProcess process;
            process.setWorkingDirectory(svnDir.absolutePath());
            process.start("svn", QStringList() << "status" << path);
            process.waitForFinished(3000);
            QString output = process.readAllStandardOutput();

            if (output.contains(" M ") || output.startsWith("M ")) isModified = true;
            if (output.contains(" A ") || output.startsWith("A ")) isAdded = true;
            if (output.contains(" D ") || output.startsWith("D ")) isDeleted = true;
            if (output.contains("C ")) isModified = true;
            continue;
        }
    }

    // Build JSON-like condition string (matching RabbitVCS format)
    QStringList conditions;
    if (hasGit) conditions << "\"is_git\":true";
    if (hasSvn) conditions << "\"is_svn\":true";
    if (isDir) conditions << "\"is_dir\":true";
    if (isFile) conditions << "\"is_file\":true";
    if (exists) conditions << "\"exists\":true";
    if (isWorkingCopy) conditions << "\"is_working_copy\":true";
    if (isInAOrAWorkingCopy) conditions << "\"is_in_a_or_a_working_copy\":true";
    if (isVersioned) conditions << "\"is_versioned\":true";
    if (isModified) conditions << "\"is_modified\":true";
    if (isAdded) conditions << "\"is_added\":true";
    if (isDeleted) conditions << "\"is_deleted\":true";

    // If not in any VCS, allow all menus
    if (!isInAOrAWorkingCopy) {
        conditions << "\"is_git\":true";
        conditions << "\"is_svn\":true";
    }

    QString result = "{" + conditions.join(",") + "}";

    qDebug() << "Generated menu conditions:" << result;
    return result;
}

QString StatusChecker::getStatusIconName(const QString &status)
{
    return statusEmblemsHash.value(status);
}

QString StatusChecker::checkStatusDirectly(const QString &path)
{
    QFileInfo fileInfo(path);
    if (!fileInfo.exists()) {
        return QString();
    }

    QDir dir(fileInfo.isDir() ? path : fileInfo.path());

    // Check Git
    QDir gitDir(dir);
    while (gitDir.exists() && !gitDir.exists(".git")) {
        if (!gitDir.cdUp()) break;
    }

    if (gitDir.exists(".git")) {
        // Don't show emblem for the repository root directory itself
        QString repoRootPath = gitDir.absolutePath();
        QString normalizedPath = QDir::cleanPath(path);
        if (normalizedPath == repoRootPath) {
            return QString();  // No emblem for Git repo root
        }

        // Use git status --porcelain --ignored to detect ignored files
        QProcess process;
        process.setWorkingDirectory(gitDir.absolutePath());
        process.start("git", QStringList() << "status" << "--porcelain" << "--ignored" << path);
        process.waitForFinished(3000);

        QString output = process.readAllStandardOutput().trimmed();
        if (!output.isEmpty()) {
            QString status = parseGitStatusLine(output, path);
            // If status shows as unversioned, check if it's actually ignored
            if (status == "unversioned" && isFileIgnored(path, "git", gitDir)) {
                return "ignored";
            }
            return status;
        }

        // File exists but not shown in status - check if tracked
        process.start("git", QStringList() << "ls-files" << path);
        process.waitForFinished(3000);
        QString lsOutput = process.readAllStandardOutput().trimmed();
        if (lsOutput.isEmpty()) {
            // Untracked file - check if ignored
            if (isFileIgnored(path, "git", gitDir)) {
                return "ignored";
            }
            return "unversioned";
        }

        // Tracked but unchanged
        return "normal";
    }

    // Check SVN
    QDir svnDir(dir);
    while (svnDir.exists() && !svnDir.exists(".svn")) {
        if (!svnDir.cdUp()) break;
    }

    if (svnDir.exists(".svn")) {
        // Don't show emblem for the repository root directory itself
        QString repoRootPath = svnDir.absolutePath();
        QString normalizedPath = QDir::cleanPath(path);
        if (normalizedPath == repoRootPath) {
            return QString();  // No emblem for SVN repo root
        }

        QProcess process;
        process.setWorkingDirectory(svnDir.absolutePath());
        process.start("svn", QStringList() << "status" << path);
        process.waitForFinished(3000);

        QString output = process.readAllStandardOutput();
        QString status = parseSvnStatusLine(output, path);

        // SVN doesn't have built-in ignore detection in status output
        // Check if file is in svn:ignore property
        if (status == "unversioned" && isFileIgnored(path, "svn", svnDir)) {
            return "ignored";
        }

        return status;
    }

    // Not in any VCS repository - no emblem
    return QString();
}

void StatusChecker::cacheDirectoryStatus(const QString &dirPath)
{
    // This is now handled asynchronously in checkStatus()
    Q_UNUSED(dirPath);
}

StatusChecker::CacheResult StatusChecker::cacheDirectoryStatusSync(const QString &dirPath)
{
    CacheResult result;
    result.dirPath = dirPath;

    QFileInfo dirInfo(dirPath);
    QDir dir(dirInfo.isDir() ? dirPath : dirInfo.path());

    // Check Git
    QDir gitDir(dir);
    while (gitDir.exists() && !gitDir.exists(".git")) {
        if (!gitDir.cdUp()) break;
    }

    if (gitDir.exists(".git")) {
        QProcess process;
        process.setWorkingDirectory(gitDir.absolutePath());
        process.start("git", QStringList() << "status" << "--porcelain");
        process.waitForFinished(5000);

        QString output = process.readAllStandardOutput();
        QStringList lines = output.split('\n', QString::SkipEmptyParts);

        QString basePath = gitDir.absolutePath();
        for (const QString &line : lines) {
            if (line.length() < 4) continue;

            QString status = parseGitStatusLine(line, line.mid(3));
            if (!status.isEmpty()) {
                QString filePath = line.mid(3);
                if (!filePath.startsWith('/')) {
                    filePath = basePath + '/' + filePath;
                }
                result.files[filePath] = status;
            }
        }

        //qDebug() << "[AsyncCache] Cached" << lines.size() << "files from Git in" << dirPath;
        return result;
    }

    // Check SVN
    QDir svnDir(dir);
    while (svnDir.exists() && !svnDir.exists(".svn")) {
        if (!svnDir.cdUp()) break;
    }

    if (svnDir.exists(".svn")) {
        QProcess process;
        process.setWorkingDirectory(svnDir.absolutePath());
        process.start("svn", QStringList() << "status");
        process.waitForFinished(5000);

        QString output = process.readAllStandardOutput();
        QStringList lines = output.split('\n', QString::SkipEmptyParts);

        for (const QString &line : lines) {
            QString status = parseSvnStatusLine(line, QString());
            if (!status.isEmpty() && line.length() > 8) {
                QString filePath = line.mid(8); // Skip status prefix
                result.files[filePath] = status;
            }
        }

        //qDebug() << "[AsyncCache] Cached" << lines.size() << "files from SVN in" << dirPath;
    }

    return result;
}

void StatusChecker::onDirectoryCacheReady()
{
    if (!cacheWatcher->isCanceled()) {
        CacheResult result = cacheWatcher->result();

        // Store results in cache with mutex protection
        QMutexLocker locker(&cacheMutex);
        dirCache[result.dirPath] = result.files;
        dirCacheTime[result.dirPath] = QDateTime::currentDateTime();

        qDebug() << "[AsyncCache] Completed for" << result.dirPath << "with" << result.files.size() << "files";
    }
}

QString StatusChecker::parseGitStatusLine(const QString &line, const QString &filePath)
{
    if (line.length() < 2) return QString();

    QChar indexStatus = line.at(0);
    QChar workTreeStatus = line.at(1);

    // Check for ignored files (!!)
    if (indexStatus == '!' && workTreeStatus == '!') {
        return "ignored";
    }

    // Priority: work tree status > index status
    QChar status = (workTreeStatus != ' ' && workTreeStatus != '?') ? workTreeStatus : indexStatus;

    if (status == 'M') return "modified";
    if (status == 'A') return "added";
    if (status == 'D') return "deleted";
    if (status == 'R') return "modified";
    if (status == 'C') return "modified";
    if (status == '?') return "unversioned";
    if (status == '!') return "missing";
    if (status == 'U') return "conflicted";

    // If both spaces, file is clean (normal)
    if (indexStatus == ' ' && workTreeStatus == ' ') return "normal";

    return "unversioned";
}

QString StatusChecker::parseSvnStatusLine(const QString &line, const QString &filePath)
{
    // SVN status format: "M       path/to/file"
    if (line.length() < 8) return "normal";

    QString status = line.left(1).trimmed();
    QString path = line.mid(8);

    if (status == "M") return "modified";
    if (status == "A") return "added";
    if (status == "D") return "deleted";
    if (status == "C") return "conflicted";
    if (status == "I") return "ignored";
    if (status == "?") return "unversioned";
    if (status == "!") return "missing";
    if (status == "~") return "complicated";
    if (status == "R") return "replaced";

    return "normal";
}

bool StatusChecker::isFileIgnored(const QString &path, const QString &vcsType, const QDir &vcsDir)
{
    QProcess process;
    process.setWorkingDirectory(vcsDir.absolutePath());

    if (vcsType == "git") {
        // Use git check-ignore to determine if file is ignored
        process.start("git", QStringList() << "check-ignore" << path);
        process.waitForFinished(3000);
        QString output = process.readAllStandardOutput().trimmed();
        return !output.isEmpty();  // If output is not empty, file is ignored
    } else if (vcsType == "svn") {
        // SVN doesn't have a direct command, check svn:ignore property
        // Get the parent directory's svn:ignore property
        QFileInfo fileInfo(path);
        QString parentDir = fileInfo.isDir() ? path : fileInfo.path();

        process.start("svn", QStringList() << "propget" << "svn:ignore" << parentDir);
        process.waitForFinished(3000);
        QString output = process.readAllStandardOutput();

        QString fileName = fileInfo.fileName();

        // Check if filename matches any ignore pattern
        QStringList ignorePatterns = output.split('\n', QString::SkipEmptyParts);
        for (const QString &pattern : ignorePatterns) {
            QString trimmedPattern = pattern.trimmed();
            if (trimmedPattern.isEmpty()) continue;

            // Simple wildcard matching
            QRegExp regex(trimmedPattern, Qt::CaseInsensitive, QRegExp::Wildcard);
            if (regex.exactMatch(fileName)) {
                return true;
            }
        }
    }

    return false;
}
