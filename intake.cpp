#include "intake.h"

Intake::Intake(QObject *parent)
    : QObject{parent}
{

}

Intake::TankStatusIterator Intake::getStartIntake()
{
    auto startTankStatus_it = _tankResultStatus.upper_bound(_tankConfig.lastIntake);
    if (std::distance(startTankStatus_it, _tankResultStatus.end()) <= MIN_STEP_COUNT_START_INTAKE)
    {
        return _tankResultStatus.end();
    }

    for (auto finishTankStatus_it = std::next(startTankStatus_it, MIN_STEP_COUNT_START_INTAKE);
         finishTankStatus_it != _tankResultStatus.end();
         ++finishTankStatus_it)
    {
        if (finishTankStatus_it->second->volume - startTankStatus_it->second->volume >= DELTA_INTAKE_VOLUME)
        {
            return finishTankStatus_it;
        }

        ++startTankStatus_it;
    }
    return _tankResultStatus.end();
}

Intake::Tank::TankStatusIterator Intake::getFinishedIntake()
{
    auto startTankStatus_it = _tankResultStatus.upper_bound(_tankConfig.lastIntake);
    if (std::distance(startTankStatus_it, _tankResultStatus.end()) <= MIN_STEP_COUNT_FINISH_INTAKE)
    {
        return _tankResultStatus.end();
    }

    for (auto finishTankStatus_it = std::next(startTankStatus_it, MIN_STEP_COUNT_FINISH_INTAKE);
         finishTankStatus_it != _tankResultStatus.end();
         ++finishTankStatus_it)
    {
        if (finishTankStatus_it->second->volume - startTankStatus_it->second->volume <= 0.0)
        {
            return startTankStatus_it;
        }

        ++startTankStatus_it;
    }
    return _tankResultStatus.end();
}

void Tank::saveIntake()
{
    auto start_it = Tank::getStartIntake();
    if (start_it == _tankResultStatus.end())
    {
        return;
    }

    auto finish_it = Tank::getFinishedIntake();
    if (finish_it == _tankResultStatus.end())
    {
        return;
    }

    //если есть начало и конец-то регистрируем приход
    if (!_tankConfig.serviceMode)
    {
        //сохраняем приход в БД НИТа
        const auto queryText =
            QString("INSERT INTO [%1].[dbo].[AddProduct] "
                    "([DateTime], [AZSCode], [TankNumber], [Product], [StartDateTime], [FinishedDateTime], [Height], [Volume], [Temp], [Density], [Mass]) "
                    "VALUES (CAST('%1' AS DATETIME2), '%2', %3, '%4', CAST('%5' AS DATETIME2), CAST('%6' AS DATETIME2), %7, %8, %9, %10, %11, %12)")
                .arg(_tankConfig.dbNitName)
                .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz"))
                .arg(_tankConfig.AZSCode)
                .arg(_tankConfig.tankNumber)
                .arg(_tankConfig.product)
                .arg(start_it->first.addSecs(_tankConfig.timeShift).toString("yyyy-MM-dd hh:mm:ss.zzz"))
                .arg(finish_it->first.addSecs(_tankConfig.timeShift).toString("yyyy-MM-dd hh:mm:ss.zzz"))
                .arg(finish_it->second->height / 10.0, 0, 'f', 1)
                .arg(finish_it->second->volume, 0, 'f', 0)
                .arg(finish_it->second->temp, 0, 'f', 1)
                .arg(finish_it->second->density, 0, 'f', 1)
                .arg(finish_it->second->mass, 0, 'f', 0);

        dbQueryExecute(queryText);
    }

    quint8 flag = static_cast<quint8>(AdditionFlag::UNKNOWN);
    for (auto it = start_it; it != finish_it; it ++)
    {
         flag = flag | static_cast<quint8>(it->second->flag);
    }

    //вставляем в нашу таблицу
    auto queryText =
            QString("INSERT INTO [TanksIntake] "
                    "([DateTime], [AZSCode], [TankNumber], [Product],   "
                    " [StartDateTime] ,[StartHeight], [StartVolume], [StartTemp], [StartDensity], [StartMass], "
                    " [FinishDateTime], [FinishHeight], [FinishStartVolume], [FinishtTemp], [FinishDensity], [FinishMass]) "
                    "VALUES (CAST('%1' AS DATETIME2), '%2', %3, '%4', "
                    " CAST('%5' AS DATETIME2), %6, %7, %8, %9, %10, %11, "
                    " CAST('%12' AS DATETIME2), %13, %14, %15, %16, %17, %18, "
                    " %19)")
                .arg(_tankConfig.dbNitName)
                .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz"))
                .arg(_tankConfig.AZSCode)
                .arg(_tankConfig.tankNumber)
                .arg(_tankConfig.product)
                .arg(start_it->first.addSecs(_tankConfig.timeShift).toString("yyyy-MM-dd hh:mm:ss.zzz"))
                .arg(start_it->second->height / 10.0, 0, 'f', 1)
                .arg(start_it->second->volume, 0, 'f', 0)
                .arg(start_it->second->temp, 0, 'f', 1)
                .arg(start_it->second->density, 0, 'f', 1)
                .arg(start_it->second->mass, 0, 'f', 0)
                .arg(finish_it->first.addSecs(_tankConfig.timeShift).toString("yyyy-MM-dd hh:mm:ss.zzz"))
                .arg(finish_it->second->height / 10.0, 0, 'f', 1)
                .arg(finish_it->second->volume, 0, 'f', 0)
                .arg(finish_it->second->temp, 0, 'f', 1)
                .arg(finish_it->second->density, 0, 'f', 1)
                .arg(finish_it->second->mass, 0, 'f', 0)
                .arg(flag);

    dbQueryExecute(queryText);

    //обновляем информацию о найденном сливае
    queryText = QString("UPDATE [TanksInfo] SET "
                        "[LastIntakeDateTime] = CAST('%1' AS DATETIME2) "
                        "WHERE ([AZSCode] = '%2') AND ([TankNumber] =%2) ")
                    .arg(finish_it->first.addSecs(_tankConfig.timeShift).toString("yyyy-MM-dd hh:mm:ss.zzz"))
                    .arg(_tankConfig.AZSCode)
                    .arg(_tankConfig.tankNumber);

    dbQueryExecute(queryText);

    _tankConfig.lastIntake = finish_it->first;
}

