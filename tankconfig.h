#pragma once

//QT
#include <QObject>
#include <QDateTime>
#include <QPair>
#include <QUrl>

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

        bool check() const
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
        INTAKE = 2,       //Прием топлива
        PUMPING_OUT = 3,  //Откачка топлива
        STUGLE = 4,       //Отстой топлива
        UNDEFINE = 0      //Не определен
    };

    enum class Mode: quint8  //Режим работы резервуара
    {
        AZS = 1,        ///< АЗС
        OIL_DEPOT = 2,  ///< Нефтебаза
        UNDEFINE = 0    ///< неопределено
    };

    ///< Вид резервуара
    enum class Type: quint8
    {
        HORIZONTAL = 1,  ///< РГС
        VERTICAL = 2,    ///< РВС
        UNDEFINE = 0     ///< неопределено
    };

    enum class ProductStatus: quint8 //Статус НП (нефтепродуктов)
    {
        UNPASPORT = 0,  ///< непаспортизированный
        PASPORT = 1,    ///< паспортизированный
        UNDEFINE = 100  ///< неопределено
    };

public:
    static Status intToStatus(quint8 status);
    static Mode intToMode(quint8 mode);
    static Type intToType(quint8 type);
    static ProductStatus intToProductStatus(quint8 productStatus);

public:
    TankConfig(const TankID& id,
               const qint64 remoteApplicantId, const qint64 remoteObjectId, const qint64 remoteTankId, const QString& name, const QString& remoteBearerToken, const QUrl& remoteBaseUrl,
               float totalVolume, float diametr, qint64 timeShift,
               Mode mode, Type type,
               const Delta& deltaMax, const Delta& deltaIntake, float deltaIntakeHeight, float deltaPumpingOutHeight, Status status,
               const QString& product, ProductStatus productStatus,
               QObject* parent = nullptr);

    const TankID& tankId() const;
    qint64 remoteTankId() const;
    qint64 remoteObjectId() const;
    qint64 remoteApplicantId() const;

    const QString& name() const;

    const QString& remoteBearerToken() const;
    const QUrl &remoteBaseUrl() const;

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

    const QString& product() const;
    ProductStatus productStatus()const;

    const QDateTime& lastMeasuments() const;
    void setLastMeasuments(const QDateTime& lastTime);

    const QDateTime& lastSave() const;
    void setLastSave(const QDateTime& lastTime);

    const QDateTime& lastSend() const;
    void setLastSend(const QDateTime& lastTime);

    const QDateTime& lastIntake() const;
    void setLastIntake(const QDateTime& lastTime);

    const QDateTime& lastSendIntake() const;
    void setLastSendIntake(const QDateTime& lastTime);

signals:
    void lastMeasuments(const LevelGaugeService::TankID& id, const QDateTime& lastTime);
    void lastSave(const LevelGaugeService::TankID& id, const QDateTime& lastTime);
    void lastIntake(const LevelGaugeService::TankID& id, const QDateTime& lastTime);
    void lastSend(const LevelGaugeService::TankID& id, const QDateTime& lastTime);
    void lastSendIntake(const LevelGaugeService::TankID& id, const QDateTime& lastTime);

private:
    TankConfig() = delete;
    Q_DISABLE_COPY_MOVE(TankConfig);

    void makeLimits();

private:    
    const TankID _id;                ///< Внутренний  ID резервуара
    const qint64 _remoteApplicantId = 0;  ///< ID организации на сервере
    const qint64 _remoteObjectId = 0;  ///< ID объекта на сервере
    const qint64 _remoteTankId = 0;  ///< ID резервуара на сервере

    const QString _name;             ///< Текстовое название резервуара (!=empty Для нефтебазы)

    const QString _remoteBearerToken;

    const QUrl _remoteBaseUrl;

    const float _totalVolume = 0.0;  ///< Объем резервуара
    const float _diametr = 0.0;      ///< Диаметр резервуара

    const qint64 _timeShift = 0;     ///< смещение времени на АЗС относительно сервера в секундах

    mutable Limits _limits;     ///< Пределы значений

    const Delta _deltaMax;      ///< максимально допустимое изменение параметров резервуара за 1 минуту при нормальной работе
    const Delta _deltaIntake;   ///< максимально допустимое изменение параметров резервуара за 1 минуту при приеме топлива

    const float _deltaIntakeHeight = 0.0; ///< пороговое значение изменения уровня топлива за 10 минут с котого считаем что произошел прием
    const float _deltaPumpingOutHeight = 0.0;

    const Status _status = Status::UNDEFINE;  ///< Текущий статус резервуара (Слив/прием/ремонт....)
    const Mode _mode = Mode::UNDEFINE;        ///< Режим работы резервуара(АЗС/Нефтебаза)
    const Type _type = Type::UNDEFINE;        ///< Вид резервуара (РГС/РВС)

    const QString _product;  ///< Название продукта (92, 95, 96,...)
    const ProductStatus _productStatus = ProductStatus::UNDEFINE; ///< Статус НП

    QDateTime _lastMeasuments = QDateTime::currentDateTime().addYears(-100); ///< время последней загруженной записи из БД Измерений
    QDateTime _lastSave = QDateTime::currentDateTime().addYears(-100);       ///< время последней сохраненной записи (время АЗС)
    QDateTime _lastSend = QDateTime::currentDateTime().addYears(-100);       ///< время последней отправленной на сервер записи
    QDateTime _lastIntake = QDateTime::currentDateTime().addYears(-100);     ///< Время последнего прихода
    QDateTime _lastSendIntake = QDateTime::currentDateTime().addYears(-100);     ///< Время последнего прихода
};

} //namespace LevelGaugeService

