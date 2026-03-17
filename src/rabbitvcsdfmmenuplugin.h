#ifndef RABBITVCSDFMMENUPLUGIN_H
#define RABBITVCSDFMMENUPLUGIN_H

#include <dfm-extension/menu/dfmextmenuplugin.h>

class RabbitVCSDFMMenuPlugin : public DFMEXT::DFMExtMenuPlugin
{
public:
    RabbitVCSDFMMenuPlugin();
    ~RabbitVCSDFMMenuPlugin();

    void initialize(DFMEXT::DFMExtMenuProxy *proxy) DFM_FAKE_OVERRIDE;
    bool buildNormalMenu(DFMEXT::DFMExtMenu *menu,
                        const std::string &currentPath,
                        const std::string &focusPath,
                        const std::list<std::string> &pathList,
                        bool onDesktop) DFM_FAKE_OVERRIDE;
    bool buildEmptyAreaMenu(DFMEXT::DFMExtMenu *menu,
                           const std::string &currentPath,
                           bool onDesktop) DFM_FAKE_OVERRIDE;

private:
    DFMEXT::DFMExtMenuProxy *proxy { nullptr };
};

#endif // RABBITVCSDFMMENUPLUGIN_H
