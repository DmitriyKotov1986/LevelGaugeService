//My
#include "Common/Common.h"

#include "tankstatuses.h"

using namespace LevelGaugeService;
using namespace Common;

TankStatuses::Status TankStatuses::intToStatus(quint8 status)
{
    switch (status)
    {
    case static_cast<quint8>(Status::REPAIR): return Status::REPAIR;
    case static_cast<quint8>(Status::RECEPTION): return Status::RECEPTION;
    case static_cast<quint8>(Status::PUMPING_OUT): return Status::PUMPING_OUT;
    case static_cast<quint8>(Status::STUGLE): return Status::STUGLE;
    case static_cast<quint8>(Status::UNDEFINE): return Status::UNDEFINE;
    default: Q_ASSERT(false);
    }

    return Status::UNDEFINE;
}

TankStatuses::TankStatuses(const TankConfig &tankConfig)
    : _tankConfig(tankConfig)
{

}

TankStatuses::TankStatusesIterator TankStatuses::add(const QDateTime &dateTime, std::unique_ptr<TankStatus>&& tankStatus)
{
    return _tankStatuses.insert({dateTime, std::move(tankStatus)}).first;
}

bool TankStatuses::loadFromDB(QSqlDatabase &db, const QDateTime &firstLoad)
{
    //т.к. приоритетное значение имеет сохранненные измерения - то сначала загружаем их
    db.transaction();
    QSqlQuery query(db);
    query.setForwardOnly(true);

    const auto queryText =
        QString("SELECT"
                    "[DateTime], [Volume], [Mass], [Density], [Height], [Temp], [Flag], [Status] "
                "FROM [TanksCalculate] "
                "WHERE "
                    "[AZSCode] = '%1' AND [TankNumber] = %2 AND [DateTime] > CAST('%3' AS DATETIME2) "
                "ORDER BY [DateTime] DESC ")
            .arg(_tankConfig.id.levelGaugeCode)
            .arg(_tankConfig.id.tankNumber)
            .arg(firstLoad.toString(DATETIME_FORMAT));

    dbQueryExecute(query, queryText);

    //сохраняем
    auto lastDateTime = QDateTime::currentDateTime();
    if (query.next())
    {
        auto tmp = std::make_unique<TankStatuses::TankStatus>();
        tmp->density = query.value("Density").toFloat();
        tmp->height = query.value("Height").toFloat() * 10; //переводим высоту обратно в мм
        tmp->mass = query.value("Mass").toFloat();
        tmp->temp = query.value("Temp").toFloat();
        tmp->volume = query.value("Volume").toFloat();
        tmp->flag = query.value("Flag").toUInt();
        tmp->status = TankStatuses::intToStatus(query.value("Status").toUInt());

        _tankResultStatuses.add(query.value("DateTime").toDateTime().addSecs(-_tankConfig.timeShift), std::move(tmp)); //время переводим во время сервера

        lastDateTime = _tankResultStatuses.rbegin()->first;
    }

    _tankConfig.lastSave = lastDateTime;
    _tankConfig.lastMeasuments = lastDateTime;

    dbCommit();
}

void TankStatuses::clear(const QDateTime &lastLoad)
{
    auto tankResultStatuses_it = _tankStatuses.begin();
    while ((tankResultStatuses_it != _tankStatuses.end()) &&
           (tankResultStatuses_it->first < lastLoad))
    {
        tankResultStatuses_it = _tankStatuses.erase(tankResultStatuses_it);
    }
}

bool TankStatuses::isEmpty() const
{
    return _tankStatuses.empty();
}

TankStatuses::TankStatusesIterator TankStatuses::begin()
{
    return _tankStatuses.begin();
}

TankStatuses::TankStatusesReversIterator TankStatuses::rbegin()
{
    return _tankStatuses.rbegin();
}

TankStatuses::TankStatusesIterator TankStatuses::end()
{
    return _tankStatuses.begin();
}

TankStatuses::TankStatusesReversIterator TankStatuses::rend()
{
    return _tankStatuses.rend();
}

TankStatuses::TankStatusesIterator TankStatuses::lowerBound(const QDateTime& dateTime)
{
    return _tankStatuses.lower_bound(dateTime);
}

TankStatuses::TankStatusesIterator TankStatuses::upperBound(const QDateTime &dateTime)
{
    return _tankStatuses.upper_bound(dateTime);
}

QString TankStatuses::additionFlagToString(quint8 flag)
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
