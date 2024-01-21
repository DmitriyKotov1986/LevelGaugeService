#ifndef TANKSTATUSES_H
#define TANKSTATUSES_H

//STL
#include <map>
#include <memory>

//QT
#include <QString>
#include <QDateTime>
#include <QSqlDatabase>

//My
#include "tankconfig.h"

namespace LevelGaugeService
{

class TankStatuses
{
public:
    enum class AdditionFlag: quint8 //флаг способа добавления записи
    {
        MEASUMENTS = 0b00000001,  //из таблицы с измерениями
        CALCULATE  = 0b00000010,  //вычислено данной службой
        MANUAL     = 0b00000100,  //вручную
        SERVICE    = 0b00001000,  //Запись не сохранена в БД НИТа из-за проводимого ТО
        CORRECTED  = 0b00010000,
        UNKNOWN    = 0b10000000,  //неизвестное состояние
        UNDEFINE   = 0b00000000
    };

    enum class Status: quint8  //Текущий статус резервуара
    {
        REPAIR = 1,       //Ремонт
        RECEPTION = 2,    //Прием топлива
        PUMPING_OUT = 3,  //Откачка топлива
        STUGLE = 4,       //Отстой топлива
        UNDEFINE = 0      //Не определен
    };

    struct TankStatus //текущий статус резервуара
    {
        float volume = -1.0;  //текущий объем
        float mass = -1.0;    //текущая масса
        float density = -1.0; //теккущая плотность
        float height = -1.0;  //текущий уровень
        float temp = -273.0;  //текущая температура
        Status status = Status::UNDEFINE; //Текущий статус резервуара
        quint8 flag = static_cast<quint8>(AdditionFlag::UNDEFINE);  //способ добавления записи
    };

private:
    using TankStatusesContainer = std::map<QDateTime, std::unique_ptr<TankStatus>>;

public:
    using TankStatusesIterator = LevelGaugeService::TankStatuses::TankStatusesContainer::iterator;
    using TankStatusesReversIterator = LevelGaugeService::TankStatuses::TankStatusesContainer::reverse_iterator;

public:
    static QString additionFlagToString(quint8 flag);
    static Status intToStatus(quint8 status);

public:   
    explicit TankStatuses(const TankConfig& tankConfig);

    TankStatuses() = delete;
    TankStatuses(const TankStatuses&) = delete;
    TankStatuses& operator =(const TankStatuses&) = delete;
    TankStatuses(const TankStatuses&&) = delete;
    TankStatuses& operator =(const TankStatuses&&) = delete;

    TankStatusesIterator add(const QDateTime& dateTime, std::unique_ptr<TankStatus>&& tankSatus);
    bool loadFromDB(QSqlDatabase& db, const QDateTime& firstLoad);
    void clear(const QDateTime& lastLoad);
    bool isEmpty() const;

    TankStatusesIterator begin();
    TankStatusesIterator end();

    TankStatusesReversIterator rbegin();
    TankStatusesReversIterator rend();

    TankStatusesIterator lowerBound(const QDateTime& dateTime);
    TankStatusesIterator upperBound(const QDateTime& dateTime);

private:
    const TankConfig _tankConfig;

    TankStatusesContainer _tankStatuses;

}; //class TankStatuses

} //namespace LevelGaugeService

#endif // TANKSTATUSES_H
