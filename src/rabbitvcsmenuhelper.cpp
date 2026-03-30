#include "rabbitvcsmenuhelper.h"

#include <QProcess>
#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <QRegularExpression>

#define VCS_SVN 0
#define VCS_GIT 1

// ── Data-driven menu infrastructure ─────────────────────────────

enum CmdMode {
    CmdPaths,          // rabbitvcs <cmd> <paths>
    CmdFirstOnly,      // rabbitvcs <cmd> <paths.first()>
    CmdNoArgs,         // rabbitvcs <cmd>
    CmdGitAdd,         // direct git add
    CmdSep,            // separator
    CmdDiffSubmenu,    // insert "差异菜单(Diff Menu)" submenu
    CmdComparePrevRev, // diff -s path@base path@working
    CmdShowChanges,    // changes path [path2]
    CmdDiffMultiple,   // diff -s <paths...>
};

// ── Menu conditions ─────────────────────────────────────────────

struct MenuConditions {
    int length;

    // Raw status
    bool isSvn, isGit, isDir, exists;
    bool isWorkingCopy, isInAOrAWorkingCopy, isVersioned;
    bool isAdded, isModified, isDeleted, isLocked, isMissing, isConflicted;
    bool hasAdded, hasModified, hasDeleted, hasMissing, hasConflicted;

    // Computed conditions
    bool canCheckout, canUpdate, canCommit, canDiffMenu, canShowLog, canAdd;
    bool canCheckForModification, canRename, canDelete, canRevert;
    bool canAnnotate, canProperties, canCreatePatch, canApplyPatch;
    bool canAddToIgnoreList, canGetLock, canBranchTag, canRelocate;
    bool canSwitch, canMerge, canImport, canExport, canUpdateToRevision;
    bool canMarkResolved, canCreateRepository, canUnlock, canCleanUp;
    bool canRepoBrowser, canInitializeRepository, canClone;
    bool canStage, canUnstage, canEditConflicts;
    bool canPush, canBranches, canTags, canRemotes, canClean, canReset;
    // Diff submenu
    bool canDiff, canDiffMultiple, canCompareTool, canCompareToolPrevRev;
    bool canCompareToolMultiple, canShowChanges;

    MenuConditions(int len, const QStringList &tl) : length(len) {
        auto has = [&](const QString &k) { return tl.contains(k); };

        isSvn = has("is_svn"); isGit = has("is_git");
        isDir = has("is_dir"); exists = has("exists");
        isWorkingCopy = has("is_working_copy");
        isInAOrAWorkingCopy = has("is_in_a_or_a_working_copy");
        isVersioned = has("is_versioned");
        isAdded = has("is_added"); isModified = has("is_modified");
        isDeleted = has("is_deleted"); isLocked = has("is_locked");
        isMissing = has("is_missing"); isConflicted = has("is_conflicted");
        hasAdded = has("has_added"); hasModified = has("has_modified");
        hasDeleted = has("has_deleted"); hasMissing = has("has_missing");
        hasConflicted = has("has_conflicted");

        bool inWC = isInAOrAWorkingCopy, ver = isVersioned;
        bool changed = isAdded || isModified || isDeleted;
        bool dirty = changed || !ver;
        bool dirDirty = hasAdded || hasModified || hasDeleted || hasMissing;

        canCheckout = (length == 1)
            ? (isGit ? inWC && ver : isDir && !isWorkingCopy) : false;
        canUpdate = inWC && ver && !isAdded;
        canCommit = (isSvn || isGit) && inWC && (dirty || isDir);
        canDiffMenu = inWC;
        canShowLog = length == 1 && inWC && ver && !isAdded;

        // add: SVN needs !isVersioned for files, Git doesn't
        if (isSvn)
            canAdd = inWC && (isDir || !ver);
        else if (isGit)
            canAdd = inWC;

        canCheckForModification = isWorkingCopy || ver;
        canRename = length == 1 && inWC && !isWorkingCopy && ver;
        canDelete = (exists || ver) && !isDeleted;
        // revert: no !isVersioned (unversioned files can't be reverted)
        canRevert = inWC && (changed || (isDir && dirDirty));
        canAnnotate = length == 1 && !isDir && inWC && ver && !isAdded;
        canProperties = length == 1 && inWC && ver;
        canCreatePatch = inWC && (dirty || (isDir && dirDirty));
        canApplyPatch = inWC;
        canAddToIgnoreList = inWC && !ver;
        canGetLock = ver;
        canBranchTag = canRelocate = canSwitch = canMerge = canGetLock;
        canImport = length == 1 && !inWC;
        canExport = length == 1;
        canUpdateToRevision = length == 1 && ver && inWC;
        canMarkResolved = inWC && ver && isConflicted;
        canCreateRepository = length == 1 && !inWC;
        canUnlock = inWC && ver && (isDir || isLocked);
        canCleanUp = ver;
        canRepoBrowser = true;
        canInitializeRepository = isDir && !inWC;
        canClone = canInitializeRepository;
        canStage = isGit && inWC;
        canUnstage = isGit && inWC && (isDir || isAdded);
        canEditConflicts = inWC && ver && isConflicted;
        canPush = canBranches = canTags = canRemotes = canClean = canReset
            = isGit && inWC;

        // Diff submenu conditions
        canDiff = length == 1 && inWC
            && (isModified || hasModified || isConflicted || hasConflicted);
        canDiffMultiple = length == 2 && ver && inWC;
        canCompareTool = canDiff;
        canCompareToolPrevRev = isSvn && inWC && length == 1;
        canCompareToolMultiple = canDiffMultiple;
        canShowChanges = inWC && ver && length <= 2;
    }
};

// ── Menu item definition ────────────────────────────────────────

struct MenuItemDef {
    const char *text;
    const char *command;
    bool MenuConditions::*cond;  // nullptr = always show
    CmdMode mode;
};

// Forward declaration
static DFMEXT::DFMExtMenu *buildDiffMenu(
    DFMEXT::DFMExtMenuProxy *proxy,
    const QStringList &paths, const MenuConditions &c,
    const QString &vcsArg);

// ── Generic menu builder ────────────────────────────────────────

static void buildMenuFromDefs(
    DFMEXT::DFMExtMenu *menu,
    DFMEXT::DFMExtMenuProxy *proxy,
    const MenuItemDef *items, int count,
    const QStringList &paths, const MenuConditions &c,
    const QString &vcsArg)
{
    for (int i = 0; i < count; i++) {
        const auto &it = items[i];

        if (it.mode == CmdSep) {
            auto sep = proxy->createAction();
            sep->setSeparator(true);
            menu->addAction(sep);
            continue;
        }

        if (it.cond && !(c.*(it.cond))) continue;

        auto act = proxy->createAction();
        act->setText(it.text);

        switch (it.mode) {
        case CmdPaths:
            act->registerTriggered([paths, cmd = QString(it.command), vcsArg](DFMEXT::DFMExtAction *, bool) {
                RabbitVCSMenuHelper::executeCommand(cmd, paths, vcsArg);
            });
            break;
        case CmdFirstOnly:
            act->registerTriggered([paths, cmd = QString(it.command), vcsArg](DFMEXT::DFMExtAction *, bool) {
                RabbitVCSMenuHelper::executeCommand(cmd, QStringList() << paths.first(), vcsArg);
            });
            break;
        case CmdNoArgs:
            act->registerTriggered([cmd = QString(it.command)](DFMEXT::DFMExtAction *, bool) {
                RabbitVCSMenuHelper::executeCommand(cmd, QStringList());
            });
            break;
        case CmdGitAdd:
            act->registerTriggered([paths](DFMEXT::DFMExtAction *, bool) {
                RabbitVCSMenuHelper::executeGitAdd(paths);
            });
            break;
        case CmdDiffSubmenu:
            act->setMenu(buildDiffMenu(proxy, paths, c, vcsArg));
            break;
        case CmdComparePrevRev:
            act->registerTriggered([paths, vcsArg](DFMEXT::DFMExtAction *, bool) {
                auto r1 = RabbitVCSMenuHelper::createPathRevisionString(paths.first(), "base");
                auto r2 = RabbitVCSMenuHelper::createPathRevisionString(paths.first(), "working");
                RabbitVCSMenuHelper::executeCommand("diff", QStringList() << "-s" << r1 << r2, vcsArg);
            });
            break;
        case CmdShowChanges:
            act->registerTriggered([paths, vcsArg](DFMEXT::DFMExtAction *, bool) {
                auto p1 = RabbitVCSMenuHelper::createPathRevisionString(paths.first(), QString());
                auto p2 = (paths.length() == 2)
                    ? RabbitVCSMenuHelper::createPathRevisionString(paths.at(1), QString())
                    : p1;
                RabbitVCSMenuHelper::executeCommand("changes", QStringList() << p1 << p2, vcsArg);
            });
            break;
        case CmdDiffMultiple:
            act->registerTriggered([paths, vcsArg](DFMEXT::DFMExtAction *, bool) {
                RabbitVCSMenuHelper::executeCommand("diff", QStringList("-s") << paths, vcsArg);
            });
            break;
        default:
            break;
        }

        menu->addAction(act);
    }
}

// ── Menu item tables ────────────────────────────────────────────

static const MenuItemDef diffMenuItems[] = {
    {"差异(Diff)",                              "diff",   &MenuConditions::canDiff,                CmdPaths},
    {"多重差异(Diff Multiple)",                 "diff",   &MenuConditions::canDiffMultiple,         CmdPaths},
    {"比较工具(Compare Tool)",                  nullptr,   &MenuConditions::canCompareTool,          CmdComparePrevRev},
    {"比较工具前一版本(Compare Tool Prev Rev)", nullptr,   &MenuConditions::canCompareToolPrevRev,  CmdComparePrevRev},
    {"比较工具多重(Compare Tool Multiple)",     nullptr,   &MenuConditions::canCompareToolMultiple,  CmdDiffMultiple},
    {"显示更改(Show Changes)",                  nullptr,   &MenuConditions::canShowChanges,          CmdShowChanges},
};

static DFMEXT::DFMExtMenu *buildDiffMenu(
    DFMEXT::DFMExtMenuProxy *proxy,
    const QStringList &paths, const MenuConditions &c,
    const QString &vcsArg)
{
    auto menu = proxy->createMenu();
    buildMenuFromDefs(menu, proxy, diffMenuItems,
                      sizeof(diffMenuItems) / sizeof(diffMenuItems[0]), paths, c, vcsArg);
    return menu;
}

static const MenuItemDef svnMenuItems[] = {
    {"检出(Checkout)",                   "checkout",  &MenuConditions::canCheckout,             CmdPaths},
    {"差异菜单(Diff Menu)",              nullptr,     &MenuConditions::canDiffMenu,             CmdDiffSubmenu},
    {"显示日志(Show Log)",               "log",       &MenuConditions::canShowLog,              CmdPaths},
    {"仓库浏览器(Repo Browser)",         "browser",   &MenuConditions::canRepoBrowser,          CmdFirstOnly},
    {"检查修改(Check for Modification)", "checkmods", &MenuConditions::canCheckForModification, CmdPaths},
    {nullptr, nullptr, nullptr, CmdSep},
    {"添加(Add)",                        "add",       &MenuConditions::canAdd,                  CmdPaths},
    {"添加到忽略列表(Add to Ignore List)","ignore",   &MenuConditions::canAddToIgnoreList,       CmdPaths},
    {nullptr, nullptr, nullptr, CmdSep},
    {"更新到版本(Update to Revision)",    "updateto",  &MenuConditions::canUpdateToRevision,     CmdPaths},
    {"重命名(Rename)",                   "rename",    &MenuConditions::canRename,               CmdPaths},
    {"删除(Delete)",                     "delete",    &MenuConditions::canDelete,               CmdPaths},
    {"还原(Revert)",                     "revert",    &MenuConditions::canRevert,               CmdPaths},
    {"编辑冲突(Edit Conflicts)",         "editconflicts",&MenuConditions::canEditConflicts,    CmdFirstOnly},
    {"标记已解决(Mark Resolved)",        "markresolved",&MenuConditions::canMarkResolved,     CmdPaths},
    {"重新定位(Relocate)",               "relocate",  &MenuConditions::canRelocate,             CmdPaths},
    {"获取锁(Get Lock)",                 "lock",      &MenuConditions::canGetLock,              CmdPaths},
    {"解锁(Unlock)",                     "unlock",    &MenuConditions::canUnlock,               CmdPaths},
    {"清理(Clean Up)",                   "cleanup",   &MenuConditions::canCleanUp,              CmdPaths},
    {nullptr, nullptr, nullptr, CmdSep},
    {"SVN 导出(SVN Export)",             "export",    &MenuConditions::canExport,               CmdPaths},
    {"创建仓库(Create Repository)",      "create",    &MenuConditions::canCreateRepository,     CmdPaths},
    {"导入(Import)",                     "import",    &MenuConditions::canImport,               CmdPaths},
    {nullptr, nullptr, nullptr, CmdSep},
    {"分支/标签(Branch/Tag)",            "branch",    &MenuConditions::canBranchTag,            CmdPaths},
    {"切换(Switch)",                     "switch",    &MenuConditions::canSwitch,               CmdPaths},
    {"合并(Merge)",                      "merge",     &MenuConditions::canMerge,                CmdPaths},
    {nullptr, nullptr, nullptr, CmdSep},
    {"创建补丁(Create Patch)",           "createpatch",&MenuConditions::canCreatePatch,        CmdPaths},
    {"应用补丁(Apply Patch)",            "applypatch", &MenuConditions::canApplyPatch,          CmdPaths},
    {"属性(Properties)",                 "property_editor",&MenuConditions::canProperties,     CmdPaths},
    {nullptr, nullptr, nullptr, CmdSep},
    {"设置(Settings)",                   "settings",  nullptr,                                  CmdNoArgs},
    {"关于(About)",                      "about",     nullptr,                                  CmdNoArgs},
};

static const MenuItemDef gitMenuItems[] = {
    {"克隆(Clone)",                      "clone",    &MenuConditions::canClone,                 CmdFirstOnly},
    {"初始化仓库(Initialize Repository)","create",   &MenuConditions::canInitializeRepository,   CmdPaths},
    {nullptr, nullptr, nullptr, CmdSep},
    {"差异菜单(Diff Menu)",              nullptr,    &MenuConditions::canDiffMenu,              CmdDiffSubmenu},
    {"显示日志(Show Log)",               "log",      &MenuConditions::canShowLog,               CmdPaths},
    {"添加(Add)",                        nullptr,    &MenuConditions::canStage,                 CmdGitAdd},
    {"取消暂存(Unstage)",                "unstage",  &MenuConditions::canUnstage,              CmdFirstOnly},
    {"添加到忽略列表(Add to Ignore List)","ignore",  &MenuConditions::canAddToIgnoreList,       CmdPaths},
    {nullptr, nullptr, nullptr, CmdSep},
    {"重命名(Rename)",                   "rename",   &MenuConditions::canRename,                CmdPaths},
    {"删除(Delete)",                     "delete",   &MenuConditions::canDelete,                CmdPaths},
    {"还原(Revert)",                     "revert",   &MenuConditions::canRevert,                CmdPaths},
    {"清理(Clean)",                      "clean",    &MenuConditions::canClean,                 CmdFirstOnly},
    {"重置(Reset)",                      "reset",    &MenuConditions::canReset,                 CmdFirstOnly},
    {"检出(Checkout)",                   "checkout", &MenuConditions::canCheckout,              CmdPaths},
    {nullptr, nullptr, nullptr, CmdSep},
    {"分支(Branches)",                   "branches", &MenuConditions::canBranches,              CmdFirstOnly},
    {"标签(Tags)",                       "tags",     &MenuConditions::canTags,                  CmdFirstOnly},
    {"远程(Remotes)",                    "remotes",  &MenuConditions::canRemotes,               CmdFirstOnly},
    {nullptr, nullptr, nullptr, CmdSep},
    {"Git 导出(Git Export)",             "export",   &MenuConditions::canExport,                CmdPaths},
    {"合并(Merge)",                      "merge",    &MenuConditions::canMerge,                 CmdPaths},
    {nullptr, nullptr, nullptr, CmdSep},
    {"注释(Annotate)",                   "annotate", &MenuConditions::canAnnotate,              CmdPaths},
    {nullptr, nullptr, nullptr, CmdSep},
    {"创建补丁(Create Patch)",           "createpatch",&MenuConditions::canCreatePatch,        CmdPaths},
    {"应用补丁(Apply Patch)",            "applypatch",&MenuConditions::canApplyPatch,         CmdPaths},
    {nullptr, nullptr, nullptr, CmdSep},
    {"设置(Settings)",                   "settings", nullptr,                                  CmdNoArgs},
    {"关于(About)",                      "about",    nullptr,                                  CmdNoArgs},
};

// ── RabbitVCSMenuHelper implementation ──────────────────────────

QString RabbitVCSMenuHelper::createPathRevisionString(const QString &path, const QString &revision)
{
    return revision.isEmpty() ? path : path + "@" + revision;
}

void RabbitVCSMenuHelper::executeCommand(const QString &command, const QStringList &paths, const QString &vcsArg)
{
    QStringList args;
    args << command;
    if (!vcsArg.isEmpty()) args << vcsArg;
    args << paths;

    QString cmdLine = "rabbitvcs " + args.join(" ");
    qWarning() << "执行RabbitVCS命令:" << cmdLine;

    QProcess p;
    p.setProgram("rabbitvcs");
    p.setArguments(args);
    p.startDetached();
    p.waitForFinished();
}

void RabbitVCSMenuHelper::executeGitAdd(const QStringList &paths)
{
    for (const QString &path : paths) {
        QFileInfo fi(path);
        QProcess p;
        p.setProgram("git");
        if (fi.isDir()) {
            p.setWorkingDirectory(path);
            p.setArguments(QStringList() << "add" << ".");
        } else {
            p.setArguments(QStringList() << "add" << fi.absoluteFilePath());
        }
        p.start();
        p.waitForFinished(-1);
    }
}

QStringList RabbitVCSMenuHelper::getTrueLists(const QString &conditions)
{
    QStringList result;
    if (conditions.isEmpty()) return result;

    QRegularExpression regex("\"([^\"]+)\":\\s*true");
    QRegularExpressionMatchIterator it = regex.globalMatch(conditions);
    while (it.hasNext())
        result << it.next().captured(1);

    return result;
}

DFMEXT::DFMExtMenu *RabbitVCSMenuHelper::getMainContextSubMenu(
        DFMEXT::DFMExtMenuProxy *proxy,
        int vcsModule,
        const QStringList &paths,
        const QStringList &trueList)
{
    auto menu = proxy->createMenu();
    MenuConditions c(paths.length(), trueList);

    if (vcsModule == VCS_SVN)
        buildMenuFromDefs(menu, proxy, svnMenuItems,
                          sizeof(svnMenuItems) / sizeof(svnMenuItems[0]), paths, c, "--vcs=svn");
    else if (vcsModule == VCS_GIT)
        buildMenuFromDefs(menu, proxy, gitMenuItems,
                          sizeof(gitMenuItems) / sizeof(gitMenuItems[0]), paths, c, "--vcs=git");

    return menu;
}

QList<DFMEXT::DFMExtAction *> RabbitVCSMenuHelper::getActionsFromConditions(
        DFMEXT::DFMExtMenuProxy *proxy,
        const QStringList &paths,
        const QString &conditions)
{
    QList<DFMEXT::DFMExtAction *> actionList;
    if (conditions.isEmpty()) return actionList;

    QStringList trueList = getTrueLists(conditions);
    MenuConditions c(paths.length(), trueList);

    qDebug() << "=== RabbitVCS Menu ==="
             << "  paths:" << paths
             << "  trueList:" << trueList;

    // Helper lambdas
    auto addSep = [&]() {
        auto sep = proxy->createAction();
        sep->setSeparator(true);
        actionList << sep;
    };

    auto addAction = [&](const char *text, const char *cmd, bool cond) {
        if (!cond) return;
        auto act = proxy->createAction();
        act->setText(text);
        act->registerTriggered([paths, cmd = QString(cmd)](DFMEXT::DFMExtAction *, bool) {
            RabbitVCSMenuHelper::executeCommand(cmd, paths);
        });
        actionList << act;
    };

    addSep();
    addAction("更新(Update)", "update", c.canUpdate);
    addAction("提交(Commit)", "commit", c.canCommit);
    addAction("推送(Push)", "push", c.canPush);
    addSep();

    // Determine which VCS submenus to show
    bool showSvn = c.isSvn || (!c.isInAOrAWorkingCopy && !c.isGit);
    bool showGit = c.isGit || (!c.isInAOrAWorkingCopy && !c.isSvn);

    auto addVcsMenu = [&](const char *text, int vcs) {
        auto act = proxy->createAction();
        act->setText(text);
        act->setMenu(getMainContextSubMenu(proxy, vcs, paths, trueList));
        actionList << act;
    };

    if (showSvn) addVcsMenu("RabbitVCS SVN", VCS_SVN);
    if (showGit) addVcsMenu("RabbitVCS Git", VCS_GIT);

    addSep();
    return actionList;
}
