#ifndef COMMONDEFINES_H
#define COMMONDEFINES_H

//STL
#include <map>

//QT
#include <QString>
#include <QHash>
#include <QDateTime>

namespace LevelGaugeService
{

struct TankID
{
    QString levelGaugeCode;
    quint8 tankNumber = 0;
};

bool operator==(const TankID& rhs, const TankID& lhs)
{
    return (rhs.tankNumber == lhs.tankNumber) && (rhs.levelGaugeCode == lhs.levelGaugeCode);
};

size_t qHash(const TankID &key, size_t seed) noexcept
{
    return qHashMulti(seed, key.levelGaugeCode, key.tankNumber);
}

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

enum class Mode: quint8  //Текущий статус резервуара
{
    REPAIR = 1,       //Ремонт
    RECEPTION = 2,    //Прием топлива
    PUMPING_OUT = 3,  //Откачка топлива
    STUGLE = 4,       //Отстой топлива
    UNDEFINE = 0      //Не определен
};

struct Status //текущий статус резервуара
{
    float volume = -1.0;  //текущий объем
    float mass = -1.0;    //текущая масса
    float density = -1.0; //теккущая плотность
    float height = -1.0;  //текущий уровень
    float temp = -273.0;  //текущая температура
    Mode tankStatus= Mode::UNDEFINE; //Текущий статус резервуара
    quint8 flag = static_cast<quint8>(AdditionFlag::UNDEFINE);  //способ добавления записи
};

using TankStatus = std::map<QDateTime, std::unique_ptr<Status>>;

inline QString additionFlagToString(quint8 flag)
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

}

#endif // COMMONDEFINES_H
