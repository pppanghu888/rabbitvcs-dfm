#ifndef RABBITVCSDFMEMBLEMS_H
#define RABBITVCSDFMEMBLEMS_H

#include <dfm-extension/emblemicon/dfmextemblemiconplugin.h>

class RabbitVCSDFMEmblems : public DFMEXT::DFMExtEmblemIconPlugin
{
public:
    RabbitVCSDFMEmblems();
    ~RabbitVCSDFMEmblems();

    DFMEXT::DFMExtEmblem locationEmblemIcons(const std::string &filePath, int systemIconCount) const DFM_FAKE_OVERRIDE;
};

#endif // RABBITVCSDFMEMBLEMS_H
