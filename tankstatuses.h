#ifndef TANKSTATUSES_H
#define TANKSTATUSES_H

//STL
#include <memory>
#include <map>

//QT
#include <QDateTime>
#include <QList>

//My
#include "tankstatus.h"

namespace LevelGaugeService
{
    using TankStatuses = std::map<QDateTime, std::unique_ptr<TankStatus>>;
    using TankStatusesList = QList<TankStatus>;

} //namespace LevelGaugeService

Q_DECLARE_METATYPE(LevelGaugeService::TankStatusesList);

#endif // TANKSTATUSES_H
