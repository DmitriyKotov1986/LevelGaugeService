#pragma once

//STL
#include <list>

//My
#include "tconfig.h"
#include "tankstatus.h"
#include "tankid.h"

namespace LevelGaugeService
{

class Intake final
{

public:
    Intake(const LevelGaugeService::TankID& id, const LevelGaugeService::TankStatus& startTankStatus,
           const LevelGaugeService::TankStatus& finishTankStatus);

    /*!
        Деструктор
    */
    ~Intake();

    const LevelGaugeService::TankID id() const;
    const LevelGaugeService::TankStatus& startTankStatus() const;
    const LevelGaugeService::TankStatus& finishTankStatus() const;

private:
    const LevelGaugeService::TankID _id;
    const LevelGaugeService::TankStatus _startTankStatus;
    const LevelGaugeService::TankStatus _finishTankStatus;

}; //class Intake

using IntakesList = std::list<Intake>;

} //namespace LevelGaugeService

Q_DECLARE_METATYPE(LevelGaugeService::IntakesList);

