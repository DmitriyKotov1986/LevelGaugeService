#include "tankid.h"

using namespace LevelGaugeService;

TankID::TankID(const QString &levelGaugeCode, quint8 tankNumber)
    :_levelGaugeCode(levelGaugeCode)
    ,_tankNumber(tankNumber)
{
    Q_ASSERT(_tankNumber != 0);
    Q_ASSERT(!levelGaugeCode.isEmpty());
}

const QString &TankID::levelGaugeCode() const
{
    Q_ASSERT(!_levelGaugeCode.isEmpty());

    return _levelGaugeCode;
}

quint8 TankID::tankNumber() const
{
    Q_ASSERT(_tankNumber != 0);

    return _tankNumber;
}

bool TankID::operator==(const TankID& other) const
{
    return (_tankNumber == other._tankNumber) && (_levelGaugeCode == other._levelGaugeCode);
}

QString TankID::toString() const
{
    return QString("%1/%2").arg(_levelGaugeCode).arg(_tankNumber);
}


