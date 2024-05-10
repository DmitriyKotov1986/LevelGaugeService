#ifndef TANKSTATUS_H
#define TANKSTATUS_H

//QT
#include <QObject>
#include <QDateTime>

//My
#include "tankconfig.h"

namespace LevelGaugeService
{

class TankStatus
{
public:
    enum class AdditionFlag: quint8 //флаг способа добавления записи
    {
        MEASUMENTS = 0b00000001,  //Значение взято из таблицы с измерениями
        CALCULATE  = 0b00000010,  //вычислено данной службой
        MANUAL     = 0b00000100,  //вручную
        SERVICE    = 0b00001000,  //Запись не сохранена в БД НИТа из-за проводимого ТО
        CORRECTED  = 0b00010000,  //Значение изменено при проверки лимитов
        UNKNOWN    = 0b10000000,  //Неизвестное состояние, используется когда потеряна связь с АЗС
        UNDEFINE   = 0b00000000   //Значение по-умолчанию
    };

    struct TankStatusData //текущий статус резервуара
    {
        QDateTime dateTime;
        float volume = -1.0;  //текущий объем
        float mass = -1.0;    //текущая масса
        float density = -1.0; //текущая плотность
        float height = -1.0;  //текущий уровень
        float temp = -273.0;  //текущая температура
        TankConfig::Status status = TankConfig::Status::UNDEFINE; //Текущий статус резервуара
        quint8 additionFlag = static_cast<quint8>(AdditionFlag::UNDEFINE);  //способ добавления записи

        bool check() const
        {
            static const float FLOAT_EPSILON = 0.0000001f;

            return (dateTime != QDateTime()) && (volume >= -FLOAT_EPSILON) && (mass >= -FLOAT_EPSILON) && (density >= 300) &&
                   (height >= -FLOAT_EPSILON) && (temp >= -100.0) && (status != TankConfig::Status::UNDEFINE);
        }
    };

public:
    static QString additionFlagToString(quint8 flag);

public:
    TankStatus();
    ~TankStatus();

    explicit TankStatus(const TankStatusData& tankStatusData);
    explicit TankStatus(TankStatusData&& tankStatusData);

    const TankStatusData& getTankStatusData() const;

    QDateTime dateTime() const;
    void setDateTime(const QDateTime& dateTime);

    float volume() const;
    void setVolume(float volume);

    float mass() const;
    void setMass(float mass);

    float density() const;
    void setDensity(float density);

    float height() const;
    void setHeight(float height);

    float temp() const;
    void setTemp(float temp);

    TankConfig::Status status() const;
    void setStatus(TankConfig::Status status);

    quint8 additionFlag() const;
    void setAdditionFlag(quint8 additionFlag);

private:
    TankStatusData _tankStatusData;

}; //class TankStatus

} //namespace LevelGaugeService

#endif // TANKSTATUS_H
