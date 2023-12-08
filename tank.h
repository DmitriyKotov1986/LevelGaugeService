#ifndef TANK_H
#define TANK_H

//QT
#include <QObject>
#include <QDateTime>
#include <QSqlDatabase>
#include <QTimer>
#include <QMap>
#include <QRandomGenerator>

//My
#include "Common/tdbloger.h"
#include "tconfig.h"

namespace LevelGaugeService
{

class Tank final: public QObject
{
    Q_OBJECT

public:
    enum class AdditionFlag: quint8 //флаг способа добавления записи
    {
        MEASUMENTS = 1,  //из таблицы с измерениями
        CALCULATE = 2,   //вычислено данной службой
        MANUAL = 4,      //вручную
        SERVICE = 8,     //Запись не сохранена в БД НИТа из-за проводимого ТО
        UNKNOW = 0       //неизвестное состояние
    };

    struct Status //текущий статус резервуара
    {
        float volume = -1.0;  //текущий объем
        float mass = -1.0;    //текущая масса
        float density = -1.0; //теккущая плотность
        float height = -1.0;  //текущий уровень
        float temp = -273.0;  //текущая температура
        AdditionFlag flag = AdditionFlag::UNKNOW ;  //способ добавления записи
    };

    struct Delta //различные дельты
    {
        float volume = 0.0;  //объем
        float mass = 0.0;    //масса
        float density = 0.0; //плотность
        float height = 0.0;  //уровень
        float temp = 0.0;    //температура
    };

    struct TankConfig //кнфигурация резервуара
    {
        friend Tank;

        QString AZSCode;      //код АЗС
        quint8 tankNumber;    //номер резервуара
        QString product;      //Вид топлива в резервуаре
        float volume = 0.0;   //Объем резервуара
        float diametr = 0.0;  //Диаметр резервуара
        QString dbNitName;    //имя БД НИТа куда нужно сохранять результаты
        qint64 timeShift = 0; //смещение времени на АЗС относительно сервера в секундах
        QDateTime lastIntake = QDateTime::currentDateTime(); //время последненего прихода (время АЗС)

    private:
        QDateTime lastMeasuments = QDateTime::currentDateTime(); //время последней загруженной записи из БД Измерений
        QDateTime lastSave = QDateTime::currentDateTime();       //время последней сохраненной записи (время АЗС)
        QDateTime lastCheck = QDateTime::currentDateTime();       //время последней проверенной записи (время АЗС)

       // QDateTime lastCheck; //время последнец проверенной записи (между которых нет разрыва)
    };

private:
    typedef QMap<QDateTime, Status> TTankStatus;

public:   
    explicit Tank(const TankConfig& tankConfig,  QObject *parent = nullptr);
    ~Tank();

//Функции необходимые для тестирования
//#ifdef QT_DEBUG
//    void addTestStatus(const Status& status) {};
//#endif

    //errors
    QString errorString();
    bool isError() const { return !_errorString.isEmpty(); }

public slots:
    void start();

private slots:
    void calculate();

signals:
    void sendLogMsg(int category, const QString& msg);

private:
    void connectToDB();
    void initFromSave();        //загружает данне о предыдыщих сохранениях из БД
    void loadFromMeasument();   //загружает новые данные из таблицы измерений
    void checkStatus();         //проверяет порядок статусов и добавляет их, если необходимо
    TTankStatus::Iterator addStatusRange(Tank::TTankStatus::Iterator start, Tank::TTankStatus::Iterator finish);
    TTankStatus::Iterator addStatusIntake(Tank::TTankStatus::Iterator start, Tank::TTankStatus::Iterator finish);
    void addStatusEnd(const QDateTime& finish);
    void addRandom(Tank::TTankStatus::Iterator it);
    void saveToNitDB();         //сохраняет данные в БД НИТа
    void saveIntake();          //Находит и сохраняет приходы
    TTankStatus::Iterator getStartIntake();    //возвращает итератор на начало приема топлива
    TTankStatus::Iterator getFinishedIntake(); //возвращает итератор на конец приема топлива

private:
    TConfig* _cnf = nullptr;
    Common::TDBLoger* _loger = nullptr;

    TankConfig _tankConfig;

    TTankStatus _tankStatus; //карта состояний резервуара

    QSqlDatabase _db;   //база данных с исходными данными
    QString _dbConnectionName;

    QString _errorString;

    QTimer* _timer = nullptr;

    QRandomGenerator *rg = QRandomGenerator::global();  //генератор случайных чисел для имитации разброса параметров при измерении в случае подстановки
};

} //namespace LevelGaugeService

#endif // TANK_H
