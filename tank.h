#ifndef TANK_H
#define TANK_H

//STL
#include <map>
#include <memory>

//QT
#include <QObject>
#include <QDateTime>
#include <QSqlDatabase>
#include <QTimer>
#include <QRandomGenerator>
#include <QPair>

//My
#include "Common/Common.h"
#include "commondefines.h"
#include "tankstatuses.h"

namespace LevelGaugeService
{

class TankTest;

class Tank
    : public QObject
{
    Q_OBJECT

    friend TankTest;

public:


    struct TankConfig //конфигурация резервуара
    {
        TankID id;

        float totalVolume = 0.0;   //Объем резервуара
        float diametr = 0.0;      //Диаметр резервуара
        qint64 timeShift = 0;     //смещение времени на АЗС относительно сервера в секундах

        Limits limits;           //Пределы значений

        Delta deltaMax;     //максимально допустимое изменение параметров резервуара за 1 минуту при нормальной работе
        Delta deltaIntake;  //максимально допустимое изменение параметров резервуара за 1 минуту при приеме топлива

        float deltaIntakeHeight = 0.0; //пороговое значение изменения уровня топлива за 10 минут с котого считаем что произошел прием

        QDateTime lastMeasuments = QDateTime::currentDateTime(); //время последней загруженной записи из БД Измерений
        QDateTime lastSave = QDateTime::currentDateTime();       //время последней сохраненной записи (время АЗС)
    };

private:
    static QString additionFlagToString(quint8 flag);

public:   
    Tank() = delete;
    Tank(const Tank&) = delete;
    Tank& operator =(const Tank&) = delete;
    Tank(const Tank&&) = delete;
    Tank& operator =(const Tank&&) = delete;

    explicit Tank(const TankID& id, const Common::DBConnectionInfo& dbConnectionInfo, QObject *parent = nullptr);
    ~Tank();

public slots:
    void start();
    void stop();

private slots:
    void calculate();

signals:
    void addStatusForSync(const LevelGaugeService::TankID& id,const QDateTime dateTime, const TankStatuses::TankStatus& status) const;
    void errorOccurred(const QString& msg);
    void finished();

private:
    void dbCommit();
    void errorDBQuery(const QSqlQuery& query);
    void dbQueryExecute(QSqlQuery &query, const QString &queryText);

    void loadTankConfig();

    void initFromSave();        //загружает данне о предыдыщих сохранениях из БД
    void loadFromMeasument();   //загружает новые данные из таблицы измерений
    void makeResultStatus();         //
    void statusDetect();
    void checkLimits(TankStatuses::TankStatusesIterator start_it);         //провеверяет лимитные ограничения статусов
    void addStatusRange(const QDateTime& targetDateTime, const TankStatuses::TankStatus& targetStatus);
    void addStatusIntake(const TankStatuses::TankStatus& targetStatus);
    void addStatusEnd();
    void addRandom(TankStatuses::TankStatusesIterator it);
    void sendNewStatuses();
    void clearTankStatus();

private:
    TankConfig _tankConfig; //Конфигурация резервуар
    TankStatuses _tankResultStatuses; //карта результирующих состояний
    TankStatuses _tankTargetStatuses; //карта целевых состояний

    QSqlDatabase _db;
    const Common::DBConnectionInfo _dbConnectionInfo;
    const QString _dbConnectionName;

    QTimer* _timer = nullptr;

    QRandomGenerator* rg = nullptr;  //генератор случайных чисел для имитации разброса параметров при измерении в случае подстановки
};

} //namespace LevelGaugeService

#endif // TANK_H
