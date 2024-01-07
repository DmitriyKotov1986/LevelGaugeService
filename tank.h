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

namespace LevelGaugeService
{

class TankTest;

class Tank
    : public QObject
{
    Q_OBJECT

    friend TankTest;

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

    struct Status //текущий статус резервуара
    {
        float volume = -1.0;  //текущий объем
        float mass = -1.0;    //текущая масса
        float density = -1.0; //теккущая плотность
        float height = -1.0;  //текущий уровень
        float temp = -273.0;  //текущая температура
        quint8 flag = static_cast<quint8>(AdditionFlag::UNDEFINE);  //способ добавления записи
    };

    struct Delta //различные дельты
    {
        float volume = 0.0;  //объем
        float mass = 0.0;    //масса
        float density = 0.0; //плотность
        float height = 0.0;  //уровень
        float temp = 0.0;    //температура
    };

    struct Limits
    {
        QPair<float, float> volume;  //объем
        QPair<float, float> mass;    //масса
        QPair<float, float> density; //плотность
        QPair<float, float> height;  //уровень
        QPair<float, float> temp;    //температура

    };

    struct TankConfig //конфигурация резервуара
    {
        qint64 id = -1;           //ID
        QString AZSCode = "999";  //код АЗС
        quint8 tankNumber = 0;    //номер резервуара
        bool serviceMode = false;
        QString product = "UD";   //Вид топлива в резервуаре
        float volume = 0.0;   //Объем резервуара
        float diametr = 0.0;  //Диаметр резервуара
        QString dbNitName;    //имя БД НИТа куда нужно сохранять результаты
        qint64 timeShift = 0; //смещение времени на АЗС относительно сервера в секундах
        QDateTime lastIntake = QDateTime::currentDateTime();     //время последненего прихода (время АЗС)
        QDateTime lastMeasuments = QDateTime::currentDateTime(); //время последней загруженной записи из БД Измерений
        QDateTime lastSave = QDateTime::currentDateTime();       //время последней сохраненной записи (время АЗС)
        QDateTime lastCheck = QDateTime::currentDateTime();      //время последней проверенной записи (время АЗС)
    };

private:
    using TankStatus = std::map<QDateTime, std::unique_ptr<Status>>;
    using TankStatusIterator = Tank::TankStatus::iterator;

public:   
    Tank() = delete;

    explicit Tank(qint64 tankID, const Common::DBConnectionInfo& dbConnectionInfo, QObject *parent = nullptr);
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
    void checkStatus();         //проверяет порядок статусов и добавляет их, если необходимо
    void checkLimits();
    TankStatusIterator addStatusRange(TankStatusIterator start, TankStatusIterator finish);
    TankStatusIterator addStatusIntake(TankStatusIterator start, TankStatusIterator finish);
    void addStatusEnd(const QDateTime& finish);
    TankStatusIterator addStatusStart(const QDateTime& start);
    void addRandom(TankStatusIterator it);
    void saveToNitDB();         //сохраняет данные в БД НИТа
    void saveIntake();          //Находит и сохраняет приходы
    TankStatusIterator getStartIntake();    //возвращает итератор на начало приема топлива
    TankStatusIterator getFinishedIntake(); //возвращает итератор на конец приема топлива

    TankStatusIterator insertTankStatus(const QDateTime dateTime, const Status& status);

private:
    const Common::DBConnectionInfo _dbConnectionInfo;
    QSqlDatabase _db;   //база данных с исходными данными
    QString _dbConnectionName;

    TankConfig _tankConfig; //Конфигурация резервуар
    TankStatus _tankStatus; //карта состояний
    Limits _limits;

    QTimer* _timer = nullptr;

    QRandomGenerator* rg = nullptr;  //генератор случайных чисел для имитации разброса параметров при измерении в случае подстановки
};

} //namespace LevelGaugeService

#endif // TANK_H
