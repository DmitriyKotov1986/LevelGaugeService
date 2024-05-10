//My
#include "Common/common.h"

#include "intake.h"

using namespace LevelGaugeService;
using namespace Common;

static const QString CONNECTION_TO_DB_NAME = "Intake";
static const QString CONNECTION_TO_DB_NIT_NAME = "IntakeNIT";

Intake::Intake(const LevelGaugeService::TankID& id, const LevelGaugeService::TankStatus& startTankStatus,
               const LevelGaugeService::TankStatus& finishTankStatus)
    : _id(id)
    , _startTankStatus(startTankStatus)
    , _finishTankStatus(finishTankStatus)
{
}

Intake::~Intake()
{
}

const TankID Intake::id() const
{
    return _id;
}

const TankStatus &Intake::startTankStatus() const
{
    return _startTankStatus;
}

const TankStatus &Intake::finishTankStatus() const
{
    return _finishTankStatus;
}




