#include "rabbitvcsdfmemblems.h"
#include "statuschecker.h"

#include <QDebug>
#include <QString>

RabbitVCSDFMEmblems::RabbitVCSDFMEmblems()
    : DFMEXT::DFMExtEmblemIconPlugin()
{
    qDebug() << "RabbitVCS emblem plugin initialized";

    // Register emblem callback
    registerLocationEmblemIcons([this](const std::string &filePath, int systemIconCount) {
        return locationEmblemIcons(filePath, systemIconCount);
    });

    qDebug() << "RabbitVCS emblem callback registered";
}

RabbitVCSDFMEmblems::~RabbitVCSDFMEmblems()
{
    qDebug() << "RabbitVCS emblem plugin destroyed";
}

DFMEXT::DFMExtEmblem RabbitVCSDFMEmblems::locationEmblemIcons(const std::string &filePath, int systemIconCount) const
{
    static int callCount = 0;
    callCount++;

    DFMEXT::DFMExtEmblem emblem;

    QString qFilePath = QString::fromStdString(filePath);
   // qDebug() << "[Emblem" << callCount << "] Checking:" << qFilePath << "systemIconCount:" << systemIconCount;

    // Don't show emblems if there are too many system icons
    if (systemIconCount >= 4) {
        qDebug() << "[Emblem" << callCount << "] Too many system icons, skipping";
        return emblem;
    }

    QString status = StatusChecker::getInstance()->checkStatus(qFilePath, true, true, true);
    //qDebug() << "[Emblem" << callCount << "] Status:" << status;

    if (status.isEmpty()) {
        qDebug() << "[Emblem" << callCount << "] Status is empty, returning no emblem";
        return emblem;
    }

    QString iconName = StatusChecker::getInstance()->getStatusIconName(status);
    //qDebug() << "[Emblem" << callCount << "] Icon name:" << iconName;

    if (iconName.isEmpty()) {
        qDebug() << "[Emblem" << callCount << "] Icon name is empty, returning no emblem";
        return emblem;
    }

    // Calculate emblem location
    // Location types: 0=bottom-right, 1=bottom-left, 2=top-right, 3=top-left
    DFMEXT::DFMExtEmblemIconLayout::LocationType locationType;
    switch (systemIconCount) {
    case 0:
        locationType = DFMEXT::DFMExtEmblemIconLayout::LocationType::BottomRight;
        break;
    case 1:
        locationType = DFMEXT::DFMExtEmblemIconLayout::LocationType::BottomLeft;
        break;
    case 2:
        locationType = DFMEXT::DFMExtEmblemIconLayout::LocationType::TopLeft;
        break;
    case 3:
    default:
        locationType = DFMEXT::DFMExtEmblemIconLayout::LocationType::TopRight;
        break;
    }

    std::vector<DFMEXT::DFMExtEmblemIconLayout> layouts;
    layouts.push_back(DFMEXT::DFMExtEmblemIconLayout(locationType, iconName.toStdString()));
    emblem.setEmblem(layouts);

    //qDebug() << "[Emblem" << callCount << "] Returning emblem with icon:" << iconName;
    return emblem;
}
