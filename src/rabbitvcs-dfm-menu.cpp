#include <dfm-extension/dfm-extension.h>

#include "rabbitvcsdfmmenuplugin.h"
#include "rabbitvcsdfmemblems.h"

#include <QDebug>
#include <QApplication>

// Plugin instances
static DFMEXT::DFMExtMenuPlugin *g_menuPlugin = nullptr;
static DFMEXT::DFMExtEmblemIconPlugin *g_emblemPlugin = nullptr;

extern "C" void dfm_extension_initiliaze()
{
    // 只在 dde-file-manager 中加载插件，不在桌面加载
    QString appName = QApplication::applicationName();
    qDebug() << "RabbitVCS: checking application name:" << appName;

    if (appName != "dde-file-manager") {
        qDebug() << "RabbitVCS: skipping initialization for application:" << appName;
        return;
    }

    qDebug() << "Initializing RabbitVCS DFM extension plugin...";

    g_menuPlugin = new RabbitVCSDFMMenuPlugin();
    g_emblemPlugin = new RabbitVCSDFMEmblems();

    qDebug() << "RabbitVCS DFM extension plugin initialized";
}

extern "C" void dfm_extension_shutdown()
{
    qDebug() << "Shutting down RabbitVCS DFM extension plugin...";

    delete g_menuPlugin;
    delete g_emblemPlugin;

    g_menuPlugin = nullptr;
    g_emblemPlugin = nullptr;

    qDebug() << "RabbitVCS DFM extension plugin shut down";
}

extern "C" DFMEXT::DFMExtMenuPlugin *dfm_extension_menu()
{
    return g_menuPlugin;
}

extern "C" DFMEXT::DFMExtEmblemIconPlugin *dfm_extension_emblem()
{
    return g_emblemPlugin;
}
