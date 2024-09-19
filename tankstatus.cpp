#include "tankstatus.h"

using namespace LevelGaugeService;

static const float FLOAT_EPSILON = 0.0000001f;

QString TankStatus::additionFlagToString(quint8 flag)
{
    if (static_cast<quint8>(flag) == 0)
    {
        return "UNDEFINE";
    }

    QString result;
    if (static_cast<quint8>(flag) & static_cast<quint8>(AdditionFlag::MEASUMENTS))
    {
        result += "MEASUMENTS|";
    }
    else if (static_cast<quint8>(flag) & static_cast<quint8>(AdditionFlag::CALCULATE))
    {
        result += "CALCULATE|";
    }
    else if (static_cast<quint8>(flag) & static_cast<quint8>(AdditionFlag::MANUAL))
    {
        result += "MANUAL|";
    }
    else if (static_cast<quint8>(flag) & static_cast<quint8>(AdditionFlag::CORRECTED))
    {
        result += "CORRECTED|";
    }
    else if (static_cast<quint8>(flag) & static_cast<quint8>(AdditionFlag::UNKNOWN))
    {
        result += "UNKNOWN|";
    }

    return result.first(result.length() - 1);
}

//class
TankStatus::TankStatus()
{  
}

TankStatus::~TankStatus()
{
}

TankStatus::TankStatus(const TankStatusData &tankStatusData)
    : _tankStatusData(tankStatusData)
{
}

TankStatus::TankStatus(TankStatusData &&tankStatusData)
    : _tankStatusData(std::move(tankStatusData))
{
}

const TankStatus::TankStatusData &TankStatus::getTankStatusData() const
{
    return _tankStatusData;
}

const QDateTime& TankStatus::dateTime() const
{
    return _tankStatusData.dateTime;
}

void TankStatus::setDateTime(const QDateTime &dateTime)
{
    Q_ASSERT(dateTime != QDateTime());

    _tankStatusData.dateTime = dateTime;
}

float TankStatus::volume() const
{
    return _tankStatusData.volume;
}

void TankStatus::setVolume(float volume)
{
    Q_ASSERT(volume > -FLOAT_EPSILON);

    _tankStatusData.volume = volume;
}

float TankStatus::mass() const
{
    return _tankStatusData.mass;
}

void TankStatus::setMass(float mass)
{
    Q_ASSERT(mass > -FLOAT_EPSILON);

    _tankStatusData.mass = mass;
}

float TankStatus::density() const
{
    return _tankStatusData.density;
}

void TankStatus::setDensity(float density)
{
    Q_ASSERT(density > -FLOAT_EPSILON);

    _tankStatusData.density = density;
}

float TankStatus::height() const
{
    return _tankStatusData.height;
}

void TankStatus::setHeight(float height)
{
    Q_ASSERT(height > -FLOAT_EPSILON);

    _tankStatusData.height = height;
}

float TankStatus::temp() const
{
    return _tankStatusData.temp;
}

void TankStatus::setTemp(float temp)
{
    Q_ASSERT(temp > -100.0);

    _tankStatusData.temp = temp;
}

TankConfig::Status TankStatus::status() const
{
    return _tankStatusData.status;
}

void TankStatus::setStatus(TankConfig::Status status)
{
    _tankStatusData.status = status;
}

quint8 TankStatus::additionFlag() const
{
    return _tankStatusData.additionFlag;
}

void TankStatus::setAdditionFlag(quint8 additionFlag)
{
    _tankStatusData.additionFlag = additionFlag;
}

bool TankStatus::operator<(const TankStatus &status) const
{
    return dateTime() < status.dateTime();
}

