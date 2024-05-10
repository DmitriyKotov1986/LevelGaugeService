#ifndef TANKCONFIG_H
#define TANKCONFIG_H

//QT
#include <QObject>
#include <QDateTime>
#include <QPair>

//My
#include "tankid.h"

namespace LevelGaugeService
{

 //конфигурация резервуара
class TankConfig
    : public QObject
{    
    Q_OBJECT

public:
    struct Delta //различные дельты
    {
        float volume = 0.0f;  //объем
        float mass = 0.0f;    //масса
        float density = 0.0f; //плотность
        float height = 0.0f;  //уровень
        float temp = 0.0f;    //температура

        bool checkDelta() const
        {
            static const float FLOAT_EPSILON = 0.0000001f;

            return (volume >= FLOAT_EPSILON) && (mass >= FLOAT_EPSILON) &&
                   (density >= FLOAT_EPSILON) && (height >= FLOAT_EPSILON) &&
                   (temp >= FLOAT_EPSILON);
        }
    };

    struct Limits //Предельные значения
    {
        QPair<float, float> volume;  //объем
        QPair<float, float> mass;    //масса
        QPair<float, float> density; //плотность
        QPair<float, float> height;  //уровень
        QPair<float, float> temp;    //температура
    };

    enum class Status: quint8  //Текущий статус резервуара
    {
        REPAIR = 1,       //Ремонт
        INTAKE = 2,    //Прием топлива
        PUMPING_OUT = 3,  //Откачка топлива
        STUGLE = 4,       //Отстой топлива
        UNDEFINE = 0      //Не определен
    };

    enum class Mode: quint8  //Режим работы резервуара
    {
        AZS = 1,        //АЗС
        OIL_DEPOT = 2,  //Нефтебаза
        UNDEFINE = 0    //неопределено
    };

    enum class Type: quint8  //Вид резервуара
    {
        HORIZONTAL = 1,  //РГС
        VERTICAL = 2,    //РВС
        UNDEFINE = 0     //неопределено
    };

    enum class ProductStatus: quint8 //Статус НП (нефтепродуктов)
    {
        UNPASPORT = 0, //непаспортизированный
        PASPORT = 1,    //паспортизированный
        UNDEFINE = 100
    };

public:
    static Status intToStatus(quint8 status);
    static Mode intToMode(quint8 mode);
    static Type intToType(quint8 type);
    static ProductStatus intToProductStatus(quint8 productStatus);

public:
    TankConfig() = delete;

    TankConfig(const TankID& id, const QString& name, float totalVolume, float diametr, qint64 timeShift, Mode mode, Type type,
               const Delta& deltaMax, const Delta& deltaIntake, float deltaIntakeHeight, float deltaPumpingOutHeight, Status status, const QString& serviceDB,
               const QString& product, ProductStatus productStatus,
               const QDateTime& lastMeasuments, const QDateTime& lastSave, const QDateTime& lastIntake, QObject* parent = nullptr);

    const TankID& tankId() const;

    const QString& name() const;

    float totalVolume() const;
    float diametr() const;

    qint64 timeShift() const;

    const Limits& limits() const;

    const Delta& deltaMax() const;
    const Delta& deltaIntake() const;

    float deltaIntakeHeight() const;
    float deltaPumpingOutHeight() const;

    Status status() const;
    Mode mode() const;
    Type type() const;

    const QString& serviceDB() const;

    const QString& product() const;
    ProductStatus productStatus()const;

    const QDateTime& lastMeasuments() const;
    void setLastMeasuments(const QDateTime& lastTime);

    const QDateTime& lastSave() const;
    void setLastSave(const QDateTime& lastTime);

    const QDateTime& lastIntake() const;
    void setLastIntake(const QDateTime& lastTime);

signals:
    void lastMeasuments(const LevelGaugeService::TankID& id, const QDateTime& lastTime);
    void lastSave(const LevelGaugeService::TankID& id, const QDateTime& lastTime);
    void lastIntake(const LevelGaugeService::TankID& id, const QDateTime& lastTime);

private:
    void makeLimits();

private:    
    const TankID _id;

    const QString _name;

    const float _totalVolume = 0.0;   //Объем резервуара
    const float _diametr = 0.0;      //Диаметр резервуара

    const qint64 _timeShift = 0;     //смещение времени на АЗС относительно сервера в секундах

    mutable Limits _limits;     //Пределы значений

    const Delta _deltaMax;     //максимально допустимое изменение параметров резервуара за 1 минуту при нормальной работе
    const Delta _deltaIntake;  //максимально допустимое изменение параметров резервуара за 1 минуту при приеме топлива

    const float _deltaIntakeHeight = 0.0; //пороговое значение изменения уровня топлива за 10 минут с котого считаем что произошел прием
    const float _deltaPumpingOutHeight = 0.0;

    const Status _status = Status::UNDEFINE;
    const Mode _mode = Mode::UNDEFINE;
    const Type _type = Type::UNDEFINE;

    const QString _serviceDB;

    const QString _product;
    const ProductStatus _productStatus = ProductStatus::UNDEFINE;

    QDateTime _lastMeasuments = QDateTime::currentDateTime(); //время последней загруженной записи из БД Измерений
    QDateTime _lastSave = QDateTime::currentDateTime();       //время последней сохраненной записи (время АЗС)
    QDateTime _lastIntake = QDateTime::currentDateTime();
};

} //namespace LevelGaugeService

#endif // TANKCONFIG_H
