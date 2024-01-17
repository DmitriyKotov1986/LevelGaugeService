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

namespace LevelGaugeService
{

class TankTest;

class Tank
    : public QObject
{
    Q_OBJECT

    friend TankTest;

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

    struct TankConfig //конфигурация резервуара
    {
        TankID id;

        float totalVolume = 0.0;   //Объем резервуара
        float diametr = 0.0;      //Диаметр резервуара
        QString dbNitName;        //имя БД НИТа куда нужно сохранять результаты
        qint64 timeShift = 0;     //смещение времени на АЗС относительно сервера в секундах
        Limits _limits;           //Пределы значений

        Delta deltaMax;     //максимально допустимое изменение параметров резервуара за 1 минуту при нормальной работе
        Delta deltaIntake;  //максимально допустимое изменение параметров резервуара за 1 минуту при приеме топлива

        float deltaIntakeHeight = 0.0; //пороговое значение изменения уровня топлива за 10 минут с котого считаем что произошел прием

        QDateTime lastIntake = QDateTime::currentDateTime();     //время последненего прихода (время АЗС)
        QDateTime lastMeasuments = QDateTime::currentDateTime(); //время последней загруженной записи из БД Измерений
        QDateTime lastSave = QDateTime::currentDateTime();       //время последней сохраненной записи (время АЗС)
    };

private:
    using TankStatusIterator = TankStatus::iterator;

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
    void sendLogMsg(int category, const QString& msg);
    void errorOccurred(const QString& msg);
    void finished();

private:
    bool connectToDB();
    void dbQueryExecute(const QString &queryText);
    void dbQueryExecute(QSqlQuery& query, const QString &queryText);
    void dbCommit();
    void errorDBQuery(const QSqlQuery& query);

    void loadTankConfig();
    void makeLimits();
    void initFromSave();        //загружает данне о предыдыщих сохранениях из БД
    void loadFromMeasument();   //загружает новые данные из таблицы измерений
    void makeResultStatus();         //
    void checkLimits(TankStatusIterator start_it);         //провеверяет лимитные ограничения статусов
    void addStatusRange(const QDateTime& targetDateTime, const Status& targetStatus);
    void addStatusIntake(const Status& targetStatus);
    void addStatusEnd();
    void addRandom(TankStatusIterator it);

private:
    const Common::DBConnectionInfo _dbConnectionInfo;
    QSqlDatabase _db;   //база данных с исходными данными
    QString _dbConnectionName;

    TankConfig _tankConfig; //Конфигурация резервуар
    TankStatus _tankResultStatus; //карта результирующих состояний
    TankStatus _tankTargetStatus; //карта целевых состояний

    QTimer* _timer = nullptr;

    QRandomGenerator* rg = nullptr;  //генератор случайных чисел для имитации разброса параметров при измерении в случае подстановки
};

} //namespace LevelGaugeService

#endif // TANK_H
