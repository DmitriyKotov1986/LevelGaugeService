#ifndef TANKCONFIG_H
#define TANKCONFIG_H

//QT
#include <QDateTime>
#include <QPair>
#include <QSqlDatabase>

//My
#include "tankid.h"

namespace LevelGaugeService
{

 //конфигурация резервуара
class TankConfig
{    
public:
    struct Delta //различные дельты
    {
        float volume = 0.0;  //объем
        float mass = 0.0;    //масса
        float density = 0.0; //плотность
        float height = 0.0;  //уровень
        float temp = 0.0;    //температура
    };

    struct Limits //Предельные значения
    {
        QPair<float, float> volume;  //объем
        QPair<float, float> mass;    //масса
        QPair<float, float> density; //плотность
        QPair<float, float> height;  //уровень
        QPair<float, float> temp;    //температура
    };

public:
    TankConfig() = delete;

    explicit TankConfig(const TankID& id, const QString& dbConnectionName);

    bool loadFromDB();

    const TankID& tankId() const;

    float totalVolume() const;
    float diametr() const;

    qint64 timeShift() const;

    const Limits& limits() const;

    const Delta& deltaMax() const;
    const Delta& deltaIntake() const;

    float deltaIntakeHeight() const;

    const QDateTime& lastMeasuments() const;
    void setLastMeasuments(const QDateTime lastTime);

    const QDateTime& lastSave() const;
    void setLastSave(const QDateTime lastTime);

    bool isError() const;
    QString errorString();

private:
    void makeLimits();

private:
    const QString _dbConnectionName;

    const TankID _id;

    float _totalVolume = 0.0;   //Объем резервуара
    float _diametr = 0.0;      //Диаметр резервуара

    qint64 _timeShift = 0;     //смещение времени на АЗС относительно сервера в секундах

    Limits _limits;           //Пределы значений

    Delta _deltaMax;     //максимально допустимое изменение параметров резервуара за 1 минуту при нормальной работе
    Delta _deltaIntake;  //максимально допустимое изменение параметров резервуара за 1 минуту при приеме топлива

    float _deltaIntakeHeight = 0.0; //пороговое значение изменения уровня топлива за 10 минут с котого считаем что произошел прием

    QDateTime _lastMeasuments = QDateTime::currentDateTime(); //время последней загруженной записи из БД Измерений
    QDateTime _lastSave = QDateTime::currentDateTime();       //время последней сохраненной записи (время АЗС)

    QString _errorString;

};

} //namespace LevelGaugeService

#endif // TANKCONFIG_H
