#include "rabbitvcsdfmmenuplugin.h"
#include "rabbitvcsmenuhelper.h"
#include "statuschecker.h"

#include <QApplication>
#include <QDebug>
#include <QUrl>
#include <QDBusInterface>
#include <QProcess>

RabbitVCSDFMMenuPlugin::RabbitVCSDFMMenuPlugin()
    : DFMEXT::DFMExtMenuPlugin()
{
    qDebug() << "RabbitVCS menu plugin created";

    // Register initialize callback FIRST
    registerInitialize([this](DFMEXT::DFMExtMenuProxy *proxy) {
        initialize(proxy);
    });

    // Register menu callbacks
    registerBuildNormalMenu([this](DFMEXT::DFMExtMenu *menu, const std::string &currentPath,
                                   const std::string &focusPath, const std::list<std::string> &pathList,
                                   bool onDesktop) {
        return buildNormalMenu(menu, currentPath, focusPath, pathList, onDesktop);
    });

    registerBuildEmptyAreaMenu([this](DFMEXT::DFMExtMenu *menu, const std::string &currentPath, bool onDesktop) {
        return buildEmptyAreaMenu(menu, currentPath, onDesktop);
    });

    qDebug() << "RabbitVCS menu callbacks registered";

    // Start RabbitVCS D-Bus service if not running
    QDBusInterface iface("org.google.code.rabbitvcs.RabbitVCS.Checker",
                         "/org/google/code/rabbitvcs/StatusChecker",
                         "org.google.code.rabbitvcs.StatusChecker");
    if (!iface.isValid()) {
        qDebug() << "RabbitVCS service not available, attempting to start with python2...";
        QProcess p;
        p.setProgram("/usr/bin/python2");
        p.setArguments(QStringList() << "/usr/lib/python2.7/dist-packages/rabbitvcs/services/checkerservice.py");
        p.startDetached();
        p.waitForFinished(1000);
        qDebug() << "RabbitVCS service started";
    }
}

RabbitVCSDFMMenuPlugin::~RabbitVCSDFMMenuPlugin()
{
    qDebug() << "RabbitVCS menu plugin destroyed";
}

void RabbitVCSDFMMenuPlugin::initialize(DFMEXT::DFMExtMenuProxy *proxy)
{
    this->proxy = proxy;
    qDebug() << "RabbitVCS menu plugin initialized";
}

bool RabbitVCSDFMMenuPlugin::buildNormalMenu(DFMEXT::DFMExtMenu *menu,
                                             const std::string &currentPath,
                                             const std::string &focusPath,
                                             const std::list<std::string> &pathList,
                                             bool onDesktop)
{
    qDebug() << "=== buildNormalMenu called ===";
    qDebug() << "  currentPath:" << QString::fromStdString(currentPath);
    qDebug() << "  focusPath:" << QString::fromStdString(focusPath);
    qDebug() << "  pathList count:" << pathList.size();
    qDebug() << "  onDesktop:" << onDesktop;

    if (!proxy) {
        qWarning() << "Menu proxy is not initialized";
        return false;
    }

    // Convert paths to QStringList
    QStringList paths;
    if (pathList.empty()) {
        // Use focusPath if no selection
        QString path = QString::fromStdString(focusPath);
        if (path.startsWith("file:///")) {
            path = QUrl(path).path();
        }
        paths << path;
    } else {
        for (const auto &path : pathList) {
            QString qPath = QString::fromStdString(path);
            if (qPath.startsWith("file:///")) {
                qPath = QUrl(qPath).path();
            }
            paths << qPath;
        }
    }

    // Filter out non-file paths
    QStringList validPaths;
    for (const QString &path : paths) {
        if (path.startsWith("/") || path.startsWith("~")) {
            validPaths << path;
        }
    }

    qDebug() << "  validPaths:" << validPaths;

    if (validPaths.isEmpty()) {
        qDebug() << "  No valid paths, returning false";
        return false;
    }

    // Get menu conditions from RabbitVCS
    qDebug() << "  Calling generateMenuConditions...";
    QString conditions = StatusChecker::getInstance()->generateMenuConditions(validPaths);
    qDebug() << "  conditions:" << conditions;

    if (conditions.isEmpty()) {
        qDebug() << "No RabbitVCS conditions for paths:" << validPaths;
        return false;
    }

    // Build menu actions
    qDebug() << "  Building menu actions...";
    QList<DFMEXT::DFMExtAction *> actions = RabbitVCSMenuHelper::getActionsFromConditions(proxy, validPaths, conditions);
    qDebug() << "  Got" << actions.count() << "actions";

    if (actions.isEmpty()) {
        qDebug() << "RabbitVCS menu built with 0 actions for" << validPaths;
        return false;
    }

#ifdef RABBITVCS_USE_ACT_GROUP_ID
    // Find a reference action to insert before (e.g., "send-to" or "property")
    auto existingActions = menu->actions();
    DFMEXT::DFMExtAction *beforeAction = nullptr;
    for (const auto &action : existingActions) {
        auto actionId = action->property("actionID");
        if (actionId == "send-to" || actionId == "property") {
            beforeAction = action;
            qDebug() << "  Found reference action:" << QString::fromStdString(actionId);
            break;
        }
    }

    // Add actions to menu using insertAction for proper positioning
    if (beforeAction) {
        // Insert before the reference action
        for (DFMEXT::DFMExtAction *action : actions) {
            menu->insertAction(beforeAction, action);
        }
    } else {
        // No reference action found, append to end
        for (DFMEXT::DFMExtAction *action : actions) {
            menu->addAction(action);
        }
    }
#else
    // Without act_group_id, just append all actions to end
    for (DFMEXT::DFMExtAction *action : actions) {
        menu->addAction(action);
    }
    qDebug() << "  Added" << actions.count() << "actions to menu end (no grouping)";
#endif

    qDebug() << "RabbitVCS menu built with" << actions.count() << "actions for" << validPaths;
    return true;
}

bool RabbitVCSDFMMenuPlugin::buildEmptyAreaMenu(DFMEXT::DFMExtMenu *menu,
                                                const std::string &currentPath,
                                                bool onDesktop)
{
    if (!proxy) {
        qWarning() << "Menu proxy is not initialized";
        return false;
    }

    // Convert current path to QString
    QString path = QString::fromStdString(currentPath);
    if (path.startsWith("file:///")) {
        path = QUrl(path).path();
    }

    // Empty area menu only shows VCS-specific submenus
    QStringList paths;
    paths << path;

    // Get menu conditions
    QString conditions = StatusChecker::getInstance()->generateMenuConditions(paths);
    if (conditions.isEmpty()) {
        return false;
    }

    // Build menu actions
    QList<DFMEXT::DFMExtAction *> actions = RabbitVCSMenuHelper::getActionsFromConditions(proxy, paths, conditions);

    if (actions.isEmpty()) {
        qDebug() << "RabbitVCS empty area menu built with 0 actions";
        return false;
    }

#ifdef RABBITVCS_USE_ACT_GROUP_ID
    // Find a reference action to insert before (e.g., "property")
    auto existingActions = menu->actions();
    DFMEXT::DFMExtAction *beforeAction = nullptr;
    for (const auto &action : existingActions) {
        auto actionId = action->property("actionID");
        if (actionId == "property") {
            beforeAction = action;
            qDebug() << "  Found reference action:" << QString::fromStdString(actionId);
            break;
        }
    }

    // Add actions to menu using insertAction for proper positioning
    if (beforeAction) {
        // Insert before the reference action
        for (DFMEXT::DFMExtAction *action : actions) {
            menu->insertAction(beforeAction, action);
        }
    } else {
        // No reference action found, append to end
        for (DFMEXT::DFMExtAction *action : actions) {
            menu->addAction(action);
        }
    }
#else
    // Without act_group_id, just append all actions to end
    for (DFMEXT::DFMExtAction *action : actions) {
        menu->addAction(action);
    }
    qDebug() << "  Added" << actions.count() << "actions to menu end (no grouping)";
#endif

    qDebug() << "RabbitVCS empty area menu built with" << actions.count() << "actions";
    return true;
}
