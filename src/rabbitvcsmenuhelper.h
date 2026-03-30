#ifndef RABBITVCSMENUHELPER_H
#define RABBITVCSMENUHELPER_H

#include <QString>
#include <QStringList>
#include <QList>

#include <dfm-extension/menu/dfmextmenu.h>
#include <dfm-extension/menu/dfmextaction.h>
#include <dfm-extension/menu/dfmextmenuproxy.h>

class RabbitVCSMenuHelper
{
public:
    RabbitVCSMenuHelper() = default;
    ~RabbitVCSMenuHelper() = default;

    static QList<DFMEXT::DFMExtAction *> getActionsFromConditions(
            DFMEXT::DFMExtMenuProxy *proxy,
            const QStringList &paths,
            const QString &conditions);

private:
    static QStringList getTrueLists(const QString &conditions);
    static DFMEXT::DFMExtMenu *getMainContextSubMenu(
            DFMEXT::DFMExtMenuProxy *proxy,
            int vcsModule,
            const QStringList &paths,
            const QStringList &trueList);

public:
    static void executeCommand(const QString &command, const QStringList &paths, const QString &vcsArg = QString());
    static void executeGitAdd(const QStringList &paths);
    static QString createPathRevisionString(const QString &path, const QString &revision);
};

#endif // RABBITVCSMENUHELPER_H
