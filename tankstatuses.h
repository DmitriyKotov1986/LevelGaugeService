#ifndef TANKSTATUSES_H
#define TANKSTATUSES_H

//STL
#include <memory>
#include <map>
#include <list>

//QT
#include <QDateTime>

//My
#include "tankstatus.h"

namespace LevelGaugeService
{
    using TankStatuses = std::map<QDateTime, std::unique_ptr<TankStatus>>;
    using TankStatusesList = std::list<TankStatus>;
} //namespace LevelGaugeService

Q_DECLARE_METATYPE(LevelGaugeService::TankStatusesList);

#endif // TANKSTATUSES_H
