//QT
#include <QSqlQuery>

//My
#include "Common/Common.h"

#include "tankconfig.h"

using namespace LevelGaugeService;
using namespace Common;

TankConfig::TankConfig(const TankID& id, const QString& dbConnectionName)
    : _dbConnectionName(dbConnectionName)
    , _id(id)
{
}

bool LevelGaugeService::TankConfig::loadFromDB()
{
    if (_dbConnectionName.isEmpty())
    {
        _errorString = "";
        return false;
    }
    auto db = QSqlDatabase::addDatabase(_dbConnectionName);

    Q_ASSERT(db.isOpen());

    db.transaction();
    QSqlQuery query(db);
    query.setForwardOnly(true);

    //загружаем данные об АЗС
    const QString queryText =
            QString("SELECT"
                        "[Volume], [Diametr], [LastSaveDateTime], [TimeShift], "
                        "[DeltaVolume], [DeltaMass], [DeltaDensity], [DeltaHeight], [DeltaTemp], "
                        "[DeltaIntakeVolume], [DeltaIntakeMass], [DeltaIntakeDensity], [DeltaIntakeHeight], [DeltaIntakeTemp], "
                        "[IntakeDetectHeight] "
                    "FROM [dbo].[TanksInfo]"
                    "WHERE [AZSCode] = '%1' AND [TankNumber] = %2")
            .arg(_id.levelGaugeCode())
            .arg(_id.tankNumber());

    if (!query.exec(queryText))
    {
        _errorString = executeDBErrorString(db, query);

        db.rollback();

        return false;
    }

    if (query.next())
    {
        _totalVolume = query.value("Volume").toFloat();
        _diametr = query.value("Diametr").toFloat();
        _lastSave = query.value("LastSaveDateTime").toDateTime();
        _lastMeasuments = query.value("LastSaveDateTime").toDateTime();
        _timeShift = query.value("TimeShift").toInt(); //cмещение времени относительно сервера

        _deltaMax.volume = query.value("DeltaVolum").toFloat();
        _deltaMax.mass = query.value("DeltaMass").toFloat();
        _deltaMax.density = query.value("DeltaDensity").toFloat();
        _deltaMax.height = query.value("DeltaHeight").toFloat();
        _deltaMax.temp = query.value("DeltaTemp").toFloat();

        _deltaIntake.volume = query.value("DeltaIntakeVolum").toFloat();
        _deltaIntake.mass = query.value("DeltaIntakeMass").toFloat();
        _deltaIntake.density = query.value("DeltaIntakeDensity").toFloat();
        _deltaIntake.height = query.value("DeltaIntakeHeight").toFloat();
        _deltaIntake.temp = query.value("DeltaIntakeTemp").toFloat();

        _deltaIntakeHeight = query.value("IntakeDetectHeight").toFloat();
    }
    else
    {
        _errorString = QString("Cannot load tank configuration with AZSCode = %1 TankNumber = %2")
                               .arg(_id.levelGaugeCode())
                               .arg(_id.tankNumber());

        return false;
    }

    if (!db.commit())
    {
        _errorString = commitDBErrorString(db);

        db.rollback();

        return false;
    }

    return true;
}

void TankConfig::makeLimits()
{
    _tankConfig.limits.density = std::make_pair<float>(350.0, 1200.0);
    _tankConfig.limits.height  = std::make_pair<float>(0.0, _tankConfig.diametr);
    _tankConfig.limits.mass    = std::make_pair<float>(0.0, _tankConfig.totalVolume * _tankConfig.limits.density.second);
    _tankConfig.limits.volume  = std::make_pair<float>(0.0, _tankConfig.totalVolume);
    _tankConfig.limits.temp    = std::make_pair<float>(-50.0, 100.0);
}

const TankID &TankConfig::tankId() const
{
    return _id;
}

bool TankConfig::isError() const
{
    return !_errorString.isEmpty();
}

QString TankConfig::errorString()
{
    QString result = _errorString;
    _errorString.clear();

    return result;
}
