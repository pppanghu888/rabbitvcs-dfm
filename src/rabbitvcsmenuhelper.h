#ifndef RABBITVCSMENUHELPER_H
#define RABBITVCSMENUHELPER_H

#include <QString>
#include <QStringList>
#include <QList>
#include <QHash>

#include <dfm-extension/menu/dfmextmenu.h>
#include <dfm-extension/menu/dfmextaction.h>
#include <dfm-extension/menu/dfmextmenuproxy.h>

class RabbitVCSMenuHelper
{
public:
    RabbitVCSMenuHelper() = default;
    ~RabbitVCSMenuHelper() = default;

    // Build menu actions based on conditions
    static QList<DFMEXT::DFMExtAction *> getActionsFromConditions(
            DFMEXT::DFMExtMenuProxy *proxy,
            const QStringList &paths,
            const QString &conditions);

private:
    // Parse conditions JSON to get true condition list
    static QStringList getTrueLists(const QString &conditions);

    // Create submenu actions for diff operations
    static DFMEXT::DFMExtMenu *getDiffSubMenu(
            DFMEXT::DFMExtMenuProxy *proxy,
            const QStringList &paths,
            const QStringList &trueList);

    // Create VCS-specific submenu (SVN/Git/Mercurial)
    static DFMEXT::DFMExtMenu *getMainContextSubMenu(
            DFMEXT::DFMExtMenuProxy *proxy,
            int vcsModule,
            const QStringList &paths,
            const QStringList &trueList);

    // Command execution helpers
    static void executeCommand(const QString &command, const QStringList &paths);
    static void executeGitAdd(const QStringList &paths);
    static QString createPathRevisionString(const QString &path, const QString &revision);
};

#endif // RABBITVCSMENUHELPER_H
