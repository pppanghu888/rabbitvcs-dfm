#include "rabbitvcsmenuhelper.h"
#include "statuschecker.h"

#include <QProcess>
#include <QFileInfo>
#include <QDir>
#include <QDebug>

// VCS module constants
#define VCS_SVN 0
#define VCS_GIT 1

QString RabbitVCSMenuHelper::createPathRevisionString(const QString &path, const QString &revision)
{
    if (!revision.isEmpty()) {
        return path + "@" + revision;
    }
    return path;
}

void RabbitVCSMenuHelper::executeCommand(const QString &command, const QStringList &paths)
{
    QString cmdLine = "rabbitvcs " + command + " " + paths.join(" ");
    qWarning() << "执行RabbitVCS命令:" << cmdLine;
    qWarning() << "  命令:" << command;
    qWarning() << "  路径:" << paths;

    QProcess p;
    p.setProgram("rabbitvcs");
    p.setArguments(QStringList() << command << paths);
    p.startDetached();
    p.waitForFinished();
}

void RabbitVCSMenuHelper::executeGitAdd(const QStringList &paths)
{
    qWarning() << "执行Git Add命令";
    qWarning() << "  路径:" << paths;

    for (const QString &path : paths) {
        QFileInfo fileInfo(path);

        if (fileInfo.isDir()) {
            // 目录：在目录内执行 git add .
            qWarning() << "  目录，执行: git add . 在" << path;

            QProcess p;
            p.setWorkingDirectory(path);
            p.setProgram("git");
            p.setArguments(QStringList() << "add" << ".");
            p.start();
            p.waitForFinished(-1);  // 无限等待

            QString output = p.readAllStandardOutput();
            QString error = p.readAllStandardError();

            if (p.exitCode() == 0) {
                qWarning() << "  Git add . 成功";
            } else {
                qWarning() << "  Git add . 失败:" << error;
            }
        } else {
            // 文件：找到git仓库根目录，然后执行 git add 相对路径
            QString filePath = fileInfo.absoluteFilePath();

            // 获取文件所在目录
            QString dirPath = fileInfo.absolutePath();

            // 在文件所在目录查找git根目录
            QProcess gitRevParse;
            gitRevParse.setWorkingDirectory(dirPath);
            gitRevParse.setProgram("git");
            gitRevParse.setArguments(QStringList() << "rev-parse" << "--show-toplevel");
            gitRevParse.start();
            gitRevParse.waitForFinished(-1);

            QString gitRoot = QString::fromUtf8(gitRevParse.readAllStandardOutput()).trimmed();

            if (gitRevParse.exitCode() != 0 || gitRoot.isEmpty()) {
                qWarning() << "  无法找到git仓库根目录:" << gitRevParse.readAllStandardError();
                continue;
            }

            // 计算相对路径
            QString relativePath = QDir(gitRoot).relativeFilePath(filePath);

            qWarning() << "  文件，执行: git add" << relativePath << "在" << gitRoot;

            QProcess p;
            p.setWorkingDirectory(gitRoot);
            p.setProgram("git");
            p.setArguments(QStringList() << "add" << relativePath);
            p.start();
            p.waitForFinished(-1);

            QString output = p.readAllStandardOutput();
            QString error = p.readAllStandardError();

            if (p.exitCode() == 0) {
                qWarning() << "  Git add 成功";
            } else {
                qWarning() << "  Git add 失败:" << error;
            }
        }
    }
}

QStringList RabbitVCSMenuHelper::getTrueLists(const QString &conditions)
{
    QStringList result;
    if (conditions.isEmpty()) {
        return result;
    }

    // Simple parsing: find all "key":true patterns
    QRegExp regex("\"([^\"]+)\":\\s*true");
    int pos = 0;
    while ((pos = regex.indexIn(conditions, pos)) >= 0) {
        QString key = regex.cap(1);
        result << key;
        pos += regex.matchedLength();
    }

    qDebug() << "  Parsed true conditions:" << result;
    return result;
}

DFMEXT::DFMExtMenu *RabbitVCSMenuHelper::getDiffSubMenu(
        DFMEXT::DFMExtMenuProxy *proxy,
        const QStringList &paths,
        const QStringList &trueList)
{
    auto menu = proxy->createMenu();

    int length = paths.length();
    bool isInAOrAWorkingCopy = trueList.contains("is_in_a_or_a_working_copy");
    bool isModified = trueList.contains("is_modified");
    bool hasModified = trueList.contains("has_modified");
    bool isConflicted = trueList.contains("is_conflicted");
    bool hasConflicted = trueList.contains("has_conflicted");
    bool isVersioned = trueList.contains("is_versioned");
    bool isSvn = trueList.contains("is_svn");

    bool canDiff = length == 1 && isInAOrAWorkingCopy && (isModified || hasModified || isConflicted || hasConflicted);
    bool canDiffMultiple = length == 2 && isVersioned && isInAOrAWorkingCopy;
    bool canCompareTool = canDiff;
    bool canCompareToolPreviousReversion = isSvn && isInAOrAWorkingCopy && length == 1;
    bool canCompareToolMultiple = canDiffMultiple;
    bool canShowChanges = isInAOrAWorkingCopy && isVersioned && (length == 1 || length == 2);

    // Diff
    auto diffAction = proxy->createAction();
    diffAction->setText("差异(Diff)");
    diffAction->registerTriggered([paths](DFMEXT::DFMExtAction *, bool) {
        RabbitVCSMenuHelper::executeCommand("diff", paths);
    });
    diffAction->setEnabled(canDiff);
    menu->addAction(diffAction);

    // Diff Multiple
    auto diffMultipleAction = proxy->createAction();
    diffMultipleAction->setText("多重差异(Diff Multiple)");
    diffMultipleAction->registerTriggered([paths](DFMEXT::DFMExtAction *, bool) {
        RabbitVCSMenuHelper::executeCommand("diff", paths);
    });
    diffMultipleAction->setEnabled(canDiffMultiple);
    menu->addAction(diffMultipleAction);

    // Compare Tool
    auto compareToolAction = proxy->createAction();
    compareToolAction->setText("比较工具(Compare Tool)");
    compareToolAction->registerTriggered([paths](DFMEXT::DFMExtAction *, bool) {
        auto pathrev1 = createPathRevisionString(paths.first(), "base");
        auto pathrev2 = createPathRevisionString(paths.first(), "working");
        executeCommand("diff", QStringList() << "-s" << pathrev1 << pathrev2);
    });
    compareToolAction->setEnabled(canCompareTool);
    menu->addAction(compareToolAction);

    // Compare Tool Previous Reversion
    auto compareToolPrevAction = proxy->createAction();
    compareToolPrevAction->setText("比较工具前一版本(Compare Tool Previous Reversion)");
    compareToolPrevAction->registerTriggered([paths, isSvn](DFMEXT::DFMExtAction *, bool) {
        if (isSvn) {
            auto pathrev1 = createPathRevisionString(paths.first(), "base");
            auto pathrev2 = createPathRevisionString(paths.first(), "working");
            executeCommand("diff", QStringList() << "-s" << pathrev1 << pathrev2);
        }
    });
    compareToolPrevAction->setEnabled(canCompareToolPreviousReversion);
    menu->addAction(compareToolPrevAction);

    // Compare Tool Multiple
    auto compareToolMultipleAction = proxy->createAction();
    compareToolMultipleAction->setText("比较工具多重(Compare Tool Multiple)");
    compareToolMultipleAction->registerTriggered([paths](DFMEXT::DFMExtAction *, bool) {
        executeCommand("diff", QStringList() << "-s" << paths);
    });
    compareToolMultipleAction->setEnabled(canCompareToolMultiple);
    menu->addAction(compareToolMultipleAction);

    // Show Changes
    auto showChangesAction = proxy->createAction();
    showChangesAction->setText("显示更改(Show Changes)");
    showChangesAction->registerTriggered([paths](DFMEXT::DFMExtAction *, bool) {
        auto pathrev1 = createPathRevisionString(paths.first(), QString());
        auto pathrev2 = pathrev1;
        if (paths.length() == 2) {
            pathrev2 = createPathRevisionString(paths.at(1), QString());
        }
        executeCommand("changes", QStringList() << pathrev1 << pathrev2);
    });
    showChangesAction->setEnabled(canShowChanges);
    menu->addAction(showChangesAction);

    return menu;
}

DFMEXT::DFMExtMenu *RabbitVCSMenuHelper::getMainContextSubMenu(
        DFMEXT::DFMExtMenuProxy *proxy,
        int vcsModule,
        const QStringList &paths,
        const QStringList &trueList)
{
    auto menu = proxy->createMenu();

    int length = paths.length();
    bool isSvn = trueList.contains("is_svn");
    bool isGit = trueList.contains("is_git");
    bool isDir = trueList.contains("is_dir");
    bool isFile = trueList.contains("is_file");
    bool exists = trueList.contains("exists");
    bool isWorkingCopy = trueList.contains("is_working_copy");
    bool isInAOrAWorkingCopy = trueList.contains("is_in_a_or_a_working_copy");
    bool isVersioned = trueList.contains("is_versioned");
    bool isNormal = trueList.contains("is_normal");
    bool isAdded = trueList.contains("is_added");
    bool isModified = trueList.contains("is_modified");
    bool isDeleted = trueList.contains("is_deleted");
    bool isIgnored = trueList.contains("is_ignored");
    bool isLocked = trueList.contains("is_locked");
    bool isMissing = trueList.contains("is_missing");
    bool isConflicted = trueList.contains("is_conflicted");
    bool isObstructed = trueList.contains("is_obstructed");
    bool hasUnversioned = trueList.contains("has_unversioned");
    bool hasAdded = trueList.contains("has_added");
    bool hasModified = trueList.contains("has_modified");
    bool hasDeleted = trueList.contains("has_deleted");
    bool hasIgnored = trueList.contains("has_ignored");
    bool hasMissing = trueList.contains("has_missing");
    bool hasConflicted = trueList.contains("has_conflicted");
    bool hasObstructed = trueList.contains("has_obstructed");

    // Calculate menu item visibility conditions
    auto canCheckoutFunc = [&]() -> bool {
        if (length == 1) {
            if (isGit) {
                return isInAOrAWorkingCopy && isVersioned;
            } else {
                return isDir && !isWorkingCopy;
            }
        }
        return false;
    };
    bool canCheckout = canCheckoutFunc();
    bool canUpdate = isInAOrAWorkingCopy && isVersioned && !isAdded;
    auto canCommitFunc = [&]() -> bool {
        if (isSvn || isGit) {
            if (isInAOrAWorkingCopy) {
                if (isAdded || isModified || isDeleted || !isVersioned) {
                    return true;
                } else {
                    if (isDir) return true;
                }
            }
        }
        return false;
    };
    bool canCommit = canCommitFunc();
    bool canDiffMenu = isInAOrAWorkingCopy;
    bool canShowLog = length == 1 && isInAOrAWorkingCopy && isVersioned && !isAdded;
    auto canAddFunc = [&]() -> bool {
        // SVN: Add unversioned files
        // Git: Stage/unstage files (Git uses "stage" command for this)
        if (isSvn) {
            if (isDir && isInAOrAWorkingCopy) return true;
            if (!isDir && isInAOrAWorkingCopy && !isVersioned) return true;
        } else if (isGit) {
            // Git: can stage unversioned or modified files
            if (isDir && isInAOrAWorkingCopy) return true;
            if (!isDir && isInAOrAWorkingCopy) return true;
        }
        return false;
    };
    bool canAdd = canAddFunc();
    bool canCheckForModification = isWorkingCopy || isVersioned;
    bool canRename = length == 1 && isInAOrAWorkingCopy && !isWorkingCopy && isVersioned;
    bool canDelete = (exists || isVersioned) && !isDeleted;
    auto canRevertFunc = [&]() -> bool {
        if (isInAOrAWorkingCopy) {
            if (isAdded || isModified || isDeleted) {
                return true;
            } else {
                if (isDir && (hasAdded || hasModified || hasDeleted || hasMissing))
                    return true;
            }
        }
        return false;
    };
    bool canRevert = canRevertFunc();
    bool canAnnotate = length == 1 && !isDir && isInAOrAWorkingCopy && isVersioned && !isAdded;
    bool canProperties = length == 1 && isInAOrAWorkingCopy && isVersioned;
    auto canCreatePatchFunc = [&]() -> bool {
        if (isInAOrAWorkingCopy) {
            if (isAdded || isModified || isDeleted || !isVersioned) {
                return true;
            } else {
                if (isDir && (hasAdded || hasModified || hasDeleted || hasMissing)) {
                    return true;
                }
            }
        }
        return false;
    };
    bool canCreatePatch = canCreatePatchFunc();
    bool canApplyPatch = isInAOrAWorkingCopy;
    bool canAddToIgnoreList = isInAOrAWorkingCopy && !isVersioned;
    bool canGetLock = isVersioned;
    bool canBranchTag = canGetLock;
    bool canRelocate = canGetLock;
    bool canSwitch = canGetLock;
    bool canMerge = canGetLock;
    bool canImport = length == 1 && !isInAOrAWorkingCopy;
    bool canExport = length == 1;
    bool canUpdateToReversion = length == 1 && isVersioned && isInAOrAWorkingCopy;
    bool canMarkResolved = isInAOrAWorkingCopy && isVersioned && isConflicted;
    bool canCreateRepository = length == 1 && !isInAOrAWorkingCopy;
    bool canUnlock = isInAOrAWorkingCopy && isVersioned && (isDir || isLocked);
    bool canCleanUp = isVersioned;
    bool canBrowseTo = exists;
    bool canOpen = isFile;
    bool canRestore = hasMissing;
    bool canRepoBrowser = true;
    bool canInitializeRepository = isDir && !isInAOrAWorkingCopy;
    bool canClone = canInitializeRepository;
    bool canPush = isGit;
    bool canBranches = isGit;
    bool canTags = isGit;
    bool canRemotes = isGit;
    bool canClean = isGit;
    bool canReset = isGit;
    auto canStageFunc = [&]() -> bool {
        if (isGit) {
            if (isDir && isInAOrAWorkingCopy) {
                qDebug() << "  canStage=true (目录)";
                return true;
            } else {
                // Git文件在working copy内就可以add（包括已版本化、未版本化、已修改等所有状态）
                qDebug() << "  canStage检查 - isDir:" << isDir << "isInAOrAWorkingCopy:" << isInAOrAWorkingCopy << "isVersioned:" << isVersioned << "isModified:" << isModified << "isAdded:" << isAdded;
                if (!isDir && isInAOrAWorkingCopy) {
                    qDebug() << "  canStage=true (文件在working copy内)";
                    return true;
                }
            }
        }
        qDebug() << "  canStage=false";
        return false;
    };
    bool canStage = canStageFunc();
    auto canUnstageFunc = [&]() -> bool {
        if (isGit) {
            if (isDir && isInAOrAWorkingCopy) {
                return true;
            } else {
                if (!isDir && isInAOrAWorkingCopy && isAdded) {
                    return true;
                }
            }
        }
        return false;
    };
    bool canUnstage = canUnstageFunc();
    bool canEditConflicts = isInAOrAWorkingCopy && isVersioned && isConflicted;

    if (vcsModule == VCS_SVN) {
        // Checkout
        auto checkoutAction = proxy->createAction();
        checkoutAction->setText("检出(Checkout)");
        checkoutAction->registerTriggered([paths](DFMEXT::DFMExtAction *, bool) {
            executeCommand("checkout", paths);
        });
        checkoutAction->setEnabled(canCheckout);
        menu->addAction(checkoutAction);

        // Diff Menu
        auto diffMenuAction = proxy->createAction();
        diffMenuAction->setText("差异菜单(Diff Menu)");
        diffMenuAction->setMenu(getDiffSubMenu(proxy, paths, trueList));
        diffMenuAction->setEnabled(canDiffMenu);
        menu->addAction(diffMenuAction);

        // Show Log
        auto showLogAction = proxy->createAction();
        showLogAction->setText("显示日志(Show Log)");
        showLogAction->registerTriggered([paths](DFMEXT::DFMExtAction *, bool) {
            executeCommand("log", paths);
        });
        showLogAction->setEnabled(canShowLog);
        menu->addAction(showLogAction);

        // Repo Browser
        auto repoBrowserAction = proxy->createAction();
        repoBrowserAction->setText("仓库浏览器(Repo Browser)");
        repoBrowserAction->registerTriggered([paths](DFMEXT::DFMExtAction *, bool) {
            executeCommand("browser", QStringList() << paths.first());
        });
        repoBrowserAction->setEnabled(canRepoBrowser);
        menu->addAction(repoBrowserAction);

        // Check for Modification
        auto checkModsAction = proxy->createAction();
        checkModsAction->setText("检查修改(Check for Modification)");
        checkModsAction->registerTriggered([paths](DFMEXT::DFMExtAction *, bool) {
            executeCommand("checkmods", paths);
        });
        checkModsAction->setEnabled(canCheckForModification);
        menu->addAction(checkModsAction);

        // Separator
        auto sep1 = proxy->createAction();
        sep1->setSeparator(true);
        menu->addAction(sep1);

        // Add
        auto addAction = proxy->createAction();
        addAction->setText("添加(Add)");
        addAction->registerTriggered([paths](DFMEXT::DFMExtAction *, bool) {
            executeCommand("add", paths);
        });
        addAction->setEnabled(canAdd);
        menu->addAction(addAction);

        // Add to Ignore List
        auto ignoreAction = proxy->createAction();
        ignoreAction->setText("添加到忽略列表(Add to Ignore List)");
        ignoreAction->registerTriggered([paths](DFMEXT::DFMExtAction *, bool) {
            executeCommand("ignore", paths);
        });
        ignoreAction->setEnabled(canAddToIgnoreList);
        menu->addAction(ignoreAction);

        // Separator
        auto sep2 = proxy->createAction();
        sep2->setSeparator(true);
        menu->addAction(sep2);

        // Update to Revision
        auto updateToAction = proxy->createAction();
        updateToAction->setText("更新到版本(Update to Revision)");
        updateToAction->registerTriggered([paths](DFMEXT::DFMExtAction *, bool) {
            executeCommand("updateto", paths);
        });
        updateToAction->setEnabled(canUpdateToReversion);
        menu->addAction(updateToAction);

        // Rename
        auto renameAction = proxy->createAction();
        renameAction->setText("重命名(Rename)");
        renameAction->registerTriggered([paths](DFMEXT::DFMExtAction *, bool) {
            executeCommand("rename", paths);
        });
        renameAction->setEnabled(canRename);
        menu->addAction(renameAction);

        // Delete
        auto deleteAction = proxy->createAction();
        deleteAction->setText("删除(Delete)");
        deleteAction->registerTriggered([paths](DFMEXT::DFMExtAction *, bool) {
            executeCommand("delete", paths);
        });
        deleteAction->setEnabled(canDelete);
        menu->addAction(deleteAction);

        // Revert
        auto revertAction = proxy->createAction();
        revertAction->setText("还原(Revert)");
        revertAction->registerTriggered([paths](DFMEXT::DFMExtAction *, bool) {
            executeCommand("revert", paths);
        });
        revertAction->setEnabled(canRevert);
        menu->addAction(revertAction);

        // Edit Conflicts
        auto editConflictsAction = proxy->createAction();
        editConflictsAction->setText("编辑冲突(Edit Conflicts)");
        editConflictsAction->registerTriggered([paths](DFMEXT::DFMExtAction *, bool) {
            executeCommand("editconflicts", QStringList() << paths.first());
        });
        editConflictsAction->setEnabled(canEditConflicts);
        menu->addAction(editConflictsAction);

        // Mark Resolved
        auto markResolvedAction = proxy->createAction();
        markResolvedAction->setText("标记已解决(Mark Resolved)");
        markResolvedAction->registerTriggered([paths](DFMEXT::DFMExtAction *, bool) {
            executeCommand("markresolved", paths);
        });
        markResolvedAction->setEnabled(canMarkResolved);
        menu->addAction(markResolvedAction);

        // Relocate
        auto relocateAction = proxy->createAction();
        relocateAction->setText("重新定位(Relocate)");
        relocateAction->registerTriggered([paths](DFMEXT::DFMExtAction *, bool) {
            executeCommand("relocate", paths);
        });
        relocateAction->setEnabled(canRelocate);
        menu->addAction(relocateAction);

        // Get Lock
        auto getLockAction = proxy->createAction();
        getLockAction->setText("获取锁(Get Lock)");
        getLockAction->registerTriggered([paths](DFMEXT::DFMExtAction *, bool) {
            executeCommand("lock", paths);
        });
        getLockAction->setEnabled(canGetLock);
        menu->addAction(getLockAction);

        // Unlock
        auto unlockAction = proxy->createAction();
        unlockAction->setText("解锁(Unlock)");
        unlockAction->registerTriggered([paths](DFMEXT::DFMExtAction *, bool) {
            executeCommand("unlock", paths);
        });
        unlockAction->setEnabled(canUnlock);
        menu->addAction(unlockAction);

        // Clean Up
        auto cleanUpAction = proxy->createAction();
        cleanUpAction->setText("清理(Clean Up)");
        cleanUpAction->registerTriggered([paths](DFMEXT::DFMExtAction *, bool) {
            executeCommand("cleanup", paths);
        });
        cleanUpAction->setEnabled(canCleanUp);
        menu->addAction(cleanUpAction);

        // Separator
        auto sep3 = proxy->createAction();
        sep3->setSeparator(true);
        menu->addAction(sep3);

        // SVN Export
        auto exportAction = proxy->createAction();
        exportAction->setText("SVN 导出(SVN Export)");
        exportAction->registerTriggered([paths](DFMEXT::DFMExtAction *, bool) {
            executeCommand("export", QStringList() << "--vcs=svn" << paths);
        });
        exportAction->setEnabled(canExport);
        menu->addAction(exportAction);

        // Create Repository
        auto createRepoAction = proxy->createAction();
        createRepoAction->setText("创建仓库(Create Repository)");
        createRepoAction->registerTriggered([paths](DFMEXT::DFMExtAction *, bool) {
            executeCommand("create", QStringList() << "--vcs=svn" << paths);
        });
        createRepoAction->setEnabled(canCreateRepository);
        menu->addAction(createRepoAction);

        // Import
        auto importAction = proxy->createAction();
        importAction->setText("导入(Import)");
        importAction->registerTriggered([paths](DFMEXT::DFMExtAction *, bool) {
            executeCommand("import", paths);
        });
        importAction->setEnabled(canImport);
        menu->addAction(importAction);

        // Separator
        auto sep4 = proxy->createAction();
        sep4->setSeparator(true);
        menu->addAction(sep4);

        // Branch/Tag
        auto branchTagAction = proxy->createAction();
        branchTagAction->setText("分支/标签(Branch/Tag)");
        branchTagAction->registerTriggered([paths](DFMEXT::DFMExtAction *, bool) {
            executeCommand("branch", paths);
        });
        branchTagAction->setEnabled(canBranchTag);
        menu->addAction(branchTagAction);

        // Switch
        auto switchAction = proxy->createAction();
        switchAction->setText("切换(Switch)");
        switchAction->registerTriggered([paths](DFMEXT::DFMExtAction *, bool) {
            executeCommand("switch", paths);
        });
        switchAction->setEnabled(canSwitch);
        menu->addAction(switchAction);

        // Merge
        auto mergeAction = proxy->createAction();
        mergeAction->setText("合并(Merge)");
        mergeAction->registerTriggered([paths](DFMEXT::DFMExtAction *, bool) {
            executeCommand("merge", paths);
        });
        mergeAction->setEnabled(canMerge);
        menu->addAction(mergeAction);

        // Separator
        auto sep5 = proxy->createAction();
        sep5->setSeparator(true);
        menu->addAction(sep5);

        // Create Patch
        auto createPatchAction = proxy->createAction();
        createPatchAction->setText("创建补丁(Create Patch)");
        createPatchAction->registerTriggered([paths](DFMEXT::DFMExtAction *, bool) {
            executeCommand("createpatch", paths);
        });
        createPatchAction->setEnabled(canCreatePatch);
        menu->addAction(createPatchAction);

        // Apply Patch
        auto applyPatchAction = proxy->createAction();
        applyPatchAction->setText("应用补丁(Apply Patch)");
        applyPatchAction->registerTriggered([paths](DFMEXT::DFMExtAction *, bool) {
            executeCommand("applypatch", paths);
        });
        applyPatchAction->setEnabled(canApplyPatch);
        menu->addAction(applyPatchAction);

        // Properties
        auto propertiesAction = proxy->createAction();
        propertiesAction->setText("属性(Properties)");
        propertiesAction->registerTriggered([paths](DFMEXT::DFMExtAction *, bool) {
            executeCommand("property_editor", paths);
        });
        propertiesAction->setEnabled(canProperties);
        menu->addAction(propertiesAction);

        // Separator
        auto sep6 = proxy->createAction();
        sep6->setSeparator(true);
        menu->addAction(sep6);

        // Settings
        auto settingsAction = proxy->createAction();
        settingsAction->setText("设置(Settings)");
        settingsAction->registerTriggered([paths](DFMEXT::DFMExtAction *, bool) {
            executeCommand("settings", QStringList());
        });
        menu->addAction(settingsAction);

        // About
        auto aboutAction = proxy->createAction();
        aboutAction->setText("关于(About)");
        aboutAction->registerTriggered([paths](DFMEXT::DFMExtAction *, bool) {
            executeCommand("about", QStringList());
        });
        menu->addAction(aboutAction);
    }
    else if (vcsModule == VCS_GIT) {
        // Clone
        auto cloneAction = proxy->createAction();
        cloneAction->setText("克隆(Clone)");
        cloneAction->registerTriggered([paths](DFMEXT::DFMExtAction *, bool) {
            executeCommand("clone", QStringList() << paths.first());
        });
        cloneAction->setEnabled(canClone);
        menu->addAction(cloneAction);

        // Initialize Repository
        auto initAction = proxy->createAction();
        initAction->setText("初始化仓库(Initialize Repository)");
        initAction->registerTriggered([paths](DFMEXT::DFMExtAction *, bool) {
            executeCommand("create", QStringList() << "--vcs=git" << paths);
        });
        initAction->setEnabled(canInitializeRepository);
        menu->addAction(initAction);

        // Separator
        auto sep1 = proxy->createAction();
        sep1->setSeparator(true);
        menu->addAction(sep1);

        // Diff Menu
        auto diffMenuAction = proxy->createAction();
        diffMenuAction->setText("差异菜单(Diff Menu)");
        diffMenuAction->setMenu(getDiffSubMenu(proxy, paths, trueList));
        diffMenuAction->setEnabled(canDiffMenu);
        menu->addAction(diffMenuAction);

        // Show Log
        auto showLogAction = proxy->createAction();
        showLogAction->setText("显示日志(Show Log)");
        showLogAction->registerTriggered([paths](DFMEXT::DFMExtAction *, bool) {
            executeCommand("log", paths);
        });
        showLogAction->setEnabled(canShowLog);
        menu->addAction(showLogAction);

        // Stage (Add for Git)
        auto stageAction = proxy->createAction();
        stageAction->setText("添加(Add)");
        stageAction->registerTriggered([paths](DFMEXT::DFMExtAction *, bool) {
            // Execute git add directly using command line
            RabbitVCSMenuHelper::executeGitAdd(paths);
        });
        stageAction->setEnabled(canStage);
        menu->addAction(stageAction);

        // Unstage
        auto unstageAction = proxy->createAction();
        unstageAction->setText("取消暂存(Unstage)");
        unstageAction->registerTriggered([paths](DFMEXT::DFMExtAction *, bool) {
            executeCommand("unstage", QStringList() << paths.first());
        });
        unstageAction->setEnabled(canUnstage);
        menu->addAction(unstageAction);

        // Add to Ignore List
        auto ignoreAction = proxy->createAction();
        ignoreAction->setText("添加到忽略列表(Add to Ignore List)");
        ignoreAction->registerTriggered([paths](DFMEXT::DFMExtAction *, bool) {
            executeCommand("ignore", paths);
        });
        ignoreAction->setEnabled(canAddToIgnoreList);
        menu->addAction(ignoreAction);

        // Separator
        auto sep2 = proxy->createAction();
        sep2->setSeparator(true);
        menu->addAction(sep2);

        // Rename
        auto renameAction = proxy->createAction();
        renameAction->setText("重命名(Rename)");
        renameAction->registerTriggered([paths](DFMEXT::DFMExtAction *, bool) {
            executeCommand("rename", paths);
        });
        renameAction->setEnabled(canRename);
        menu->addAction(renameAction);

        // Delete
        auto deleteAction = proxy->createAction();
        deleteAction->setText("删除(Delete)");
        deleteAction->registerTriggered([paths](DFMEXT::DFMExtAction *, bool) {
            executeCommand("delete", paths);
        });
        deleteAction->setEnabled(canDelete);
        menu->addAction(deleteAction);

        // Revert
        auto revertAction = proxy->createAction();
        revertAction->setText("还原(Revert)");
        revertAction->registerTriggered([paths](DFMEXT::DFMExtAction *, bool) {
            executeCommand("revert", paths);
        });
        revertAction->setEnabled(canRevert);
        menu->addAction(revertAction);

        // Clean
        auto cleanAction = proxy->createAction();
        cleanAction->setText("清理(Clean)");
        cleanAction->registerTriggered([paths](DFMEXT::DFMExtAction *, bool) {
            executeCommand("clean", QStringList() << paths.first());
        });
        cleanAction->setEnabled(canClean);
        menu->addAction(cleanAction);

        // Reset
        auto resetAction = proxy->createAction();
        resetAction->setText("重置(Reset)");
        resetAction->registerTriggered([paths](DFMEXT::DFMExtAction *, bool) {
            executeCommand("reset", QStringList() << paths.first());
        });
        resetAction->setEnabled(canReset);
        menu->addAction(resetAction);

        // Checkout
        auto checkoutAction = proxy->createAction();
        checkoutAction->setText("检出(Checkout)");
        checkoutAction->registerTriggered([paths](DFMEXT::DFMExtAction *, bool) {
            executeCommand("checkout", paths);
        });
        checkoutAction->setEnabled(canCheckout);
        menu->addAction(checkoutAction);

        // Separator
        auto sep3 = proxy->createAction();
        sep3->setSeparator(true);
        menu->addAction(sep3);

        // Branches
        auto branchesAction = proxy->createAction();
        branchesAction->setText("分支(Branches)");
        branchesAction->registerTriggered([paths](DFMEXT::DFMExtAction *, bool) {
            executeCommand("branches", QStringList() << paths.first());
        });
        branchesAction->setEnabled(canBranches);
        menu->addAction(branchesAction);

        // Tags
        auto tagsAction = proxy->createAction();
        tagsAction->setText("标签(Tags)");
        tagsAction->registerTriggered([paths](DFMEXT::DFMExtAction *, bool) {
            executeCommand("tags", QStringList() << paths.first());
        });
        tagsAction->setEnabled(canTags);
        menu->addAction(tagsAction);

        // Remotes
        auto remotesAction = proxy->createAction();
        remotesAction->setText("远程(Remotes)");
        remotesAction->registerTriggered([paths](DFMEXT::DFMExtAction *, bool) {
            executeCommand("remotes", QStringList() << paths.first());
        });
        remotesAction->setEnabled(canRemotes);
        menu->addAction(remotesAction);

        // Separator
        auto sep4 = proxy->createAction();
        sep4->setSeparator(true);
        menu->addAction(sep4);

        // Git Export
        auto exportAction = proxy->createAction();
        exportAction->setText("Git 导出(Git Export)");
        exportAction->registerTriggered([paths](DFMEXT::DFMExtAction *, bool) {
            executeCommand("export", QStringList() << "--vcs=git" << paths);
        });
        exportAction->setEnabled(canExport);
        menu->addAction(exportAction);

        // Merge
        auto mergeAction = proxy->createAction();
        mergeAction->setText("合并(Merge)");
        mergeAction->registerTriggered([paths](DFMEXT::DFMExtAction *, bool) {
            executeCommand("merge", paths);
        });
        mergeAction->setEnabled(canMerge);
        menu->addAction(mergeAction);

        // Separator
        auto sep5 = proxy->createAction();
        sep5->setSeparator(true);
        menu->addAction(sep5);

        // Annotate
        auto annotateAction = proxy->createAction();
        annotateAction->setText("注释(Annotate)");
        annotateAction->registerTriggered([paths](DFMEXT::DFMExtAction *, bool) {
            executeCommand("annotate", paths);
        });
        annotateAction->setEnabled(canAnnotate);
        menu->addAction(annotateAction);

        // Separator
        auto sep6 = proxy->createAction();
        sep6->setSeparator(true);
        menu->addAction(sep6);

        // Create Patch
        auto createPatchAction = proxy->createAction();
        createPatchAction->setText("创建补丁(Create Patch)");
        createPatchAction->registerTriggered([paths](DFMEXT::DFMExtAction *, bool) {
            executeCommand("createpatch", paths);
        });
        createPatchAction->setEnabled(canCreatePatch);
        menu->addAction(createPatchAction);

        // Apply Patch
        auto applyPatchAction = proxy->createAction();
        applyPatchAction->setText("应用补丁(Apply Patch)");
        applyPatchAction->registerTriggered([paths](DFMEXT::DFMExtAction *, bool) {
            executeCommand("applypatch", paths);
        });
        applyPatchAction->setEnabled(canApplyPatch);
        menu->addAction(applyPatchAction);

        // Separator
        auto sep7 = proxy->createAction();
        sep7->setSeparator(true);
        menu->addAction(sep7);

        // Settings
        auto settingsAction = proxy->createAction();
        settingsAction->setText("设置(Settings)");
        settingsAction->registerTriggered([paths](DFMEXT::DFMExtAction *, bool) {
            executeCommand("settings", QStringList());
        });
        menu->addAction(settingsAction);

        // About
        auto aboutAction = proxy->createAction();
        aboutAction->setText("关于(About)");
        aboutAction->registerTriggered([paths](DFMEXT::DFMExtAction *, bool) {
            executeCommand("about", QStringList());
        });
        menu->addAction(aboutAction);
    }

    return menu;
}

QList<DFMEXT::DFMExtAction *> RabbitVCSMenuHelper::getActionsFromConditions(
        DFMEXT::DFMExtMenuProxy *proxy,
        const QStringList &paths,
        const QString &conditions)
{
    QList<DFMEXT::DFMExtAction *> actionList;

    if (conditions.isEmpty()) {
        qWarning() << "Conditions is empty!";
        return actionList;
    }

    QStringList trueList = getTrueLists(conditions);
    qDebug() << "=== RabbitVCS Menu ===";
    qDebug() << "  paths:" << paths;
    qDebug() << "  conditions:" << conditions;
    qDebug() << "  trueList:" << trueList;

    bool isGit = trueList.contains("is_git");
    bool isSvn = trueList.contains("is_svn");
    bool isDir = trueList.contains("is_dir");
    bool isDeleted = trueList.contains("is_deleted");
    bool isModified = trueList.contains("is_modified");
    bool isInAOrAWorkingCopy = trueList.contains("is_in_a_or_a_working_copy");
    bool isVersioned = trueList.contains("is_versioned");
    bool isAdded = trueList.contains("is_added");

    qDebug() << "  isGit:" << isGit << "isSvn:" << isSvn << "isInWorkingCopy:" << isInAOrAWorkingCopy;

    // Calculate visibility conditions
    auto canUpdateFunc = [&]() -> bool {
        return isInAOrAWorkingCopy && isVersioned && !isAdded;
    };
    bool canUpdate = canUpdateFunc();

    auto canCommitFunc = [&]() -> bool {
        if (isSvn || isGit) {
            if (isInAOrAWorkingCopy) {
                if (isAdded || isModified || isDeleted || !isVersioned) {
                    return true;
                } else {
                    if (isDir) return true;
                }
            }
        }
        return false;
    };
    bool canCommit = canCommitFunc();
    bool canPush = isGit;
    bool canRabbitVCS_SVN = isSvn || !isInAOrAWorkingCopy;
    bool canRabbitVCS_GIT = isGit || !isInAOrAWorkingCopy;

    // Leading separator
    auto leadSep = proxy->createAction();
    leadSep->setSeparator(true);
#ifdef RABBITVCS_USE_ACT_GROUP_ID
    leadSep->setProperty("act_group_id", "rabbitvcs");
#endif
    actionList << leadSep;

    // Update
    auto updateAction = proxy->createAction();
    updateAction->setText("更新(Update)");
#ifdef RABBITVCS_USE_ACT_GROUP_ID
    updateAction->setProperty("act_group_id", "rabbitvcs");
#endif
    updateAction->registerTriggered([paths](DFMEXT::DFMExtAction *, bool) {
        RabbitVCSMenuHelper::executeCommand("update", paths);
    });
    updateAction->setEnabled(canUpdate);
    actionList << updateAction;

    // Commit
    auto commitAction = proxy->createAction();
    commitAction->setText("提交(Commit)");
#ifdef RABBITVCS_USE_ACT_GROUP_ID
    commitAction->setProperty("act_group_id", "rabbitvcs");
#endif
    commitAction->registerTriggered([paths](DFMEXT::DFMExtAction *, bool) {
        RabbitVCSMenuHelper::executeCommand("commit", paths);
    });
    commitAction->setEnabled(canCommit);
    actionList << commitAction;

    // Push
    auto pushAction = proxy->createAction();
    pushAction->setText("推送(Push)");
#ifdef RABBITVCS_USE_ACT_GROUP_ID
    pushAction->setProperty("act_group_id", "rabbitvcs");
#endif
    pushAction->registerTriggered([paths](DFMEXT::DFMExtAction *, bool) {
        RabbitVCSMenuHelper::executeCommand("push", paths);
    });
    pushAction->setEnabled(canPush);
    actionList << pushAction;

    // Separator
    auto sep = proxy->createAction();
    sep->setSeparator(true);
#ifdef RABBITVCS_USE_ACT_GROUP_ID
    sep->setProperty("act_group_id", "rabbitvcs");
#endif
    actionList << sep;

    // RabbitVCS SVN submenu
    qDebug() << "  canRabbitVCS_SVN:" << canRabbitVCS_SVN;
    if (canRabbitVCS_SVN) {
        qDebug() << "  Adding RabbitVCS SVN submenu";
        auto svnAction = proxy->createAction();
        svnAction->setText("RabbitVCS SVN");
#ifdef RABBITVCS_USE_ACT_GROUP_ID
        svnAction->setProperty("act_group_id", "rabbitvcs");
#endif
        auto svnMenu = getMainContextSubMenu(proxy, VCS_SVN, paths, trueList);
        svnAction->setMenu(svnMenu);
        actionList << svnAction;
    }

    // RabbitVCS Git submenu
    qDebug() << "  canRabbitVCS_GIT:" << canRabbitVCS_GIT;
    if (canRabbitVCS_GIT) {
        qDebug() << "  Adding RabbitVCS Git submenu";
        auto gitAction = proxy->createAction();
        gitAction->setText("RabbitVCS Git");
#ifdef RABBITVCS_USE_ACT_GROUP_ID
        gitAction->setProperty("act_group_id", "rabbitvcs");
#endif
        auto gitMenu = getMainContextSubMenu(proxy, VCS_GIT, paths, trueList);
        gitAction->setMenu(gitMenu);
        actionList << gitAction;
    }

    // Trailing separator
    auto trailSep = proxy->createAction();
    trailSep->setSeparator(true);
#ifdef RABBITVCS_USE_ACT_GROUP_ID
    trailSep->setProperty("act_group_id", "rabbitvcs");
#endif
    actionList << trailSep;

    return actionList;
}
