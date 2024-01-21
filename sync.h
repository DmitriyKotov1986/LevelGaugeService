//*****************************************************************************
//
// Класс выполняет синхронизацию сгенерированной таблице показаний с
// БД АО НИТ
//
//*****************************************************************************
#ifndef SYNC_H
#define SYNC_H

//STL
#include <queue>

//Qt
#include <QObject>
#include <QHash>
#include <QTimer>
#include <QQueue>

//My
#include "tconfig.h"
#include "commondefines.h"
#include "tankstatuses.h"

namespace LevelGaugeService
{

class Sync final
    : public QObject
{
    Q_OBJECT

private:
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

    enum class ProductStatus //Статус НП (нефтепродуктов)
    {
        UNPASPORT = 0, //непаспортизированный
        PASPORT = 1    //паспортизированный
    };

    struct TankConfig
    {
        QString tankName = "n/a";     //Внутренее наименование резервуара
        QString dbNitName;
        qint64 timeShift = 0;
        Mode mode = Mode::UNDEFINE;  //Режим работы резервуара
        Type type = Type::UNDEFINE;  //Вид резервуара
        QString product = "UD";
        ProductStatus productStatus = ProductStatus::UNPASPORT; //Статус НП (нефтепродуктов)
        float totalVolume  = 0.0;
    };

    using TanksConfig = QHash<TankID, TankConfig>;

    struct StatusData
    {
        TankID id;
        QDateTime dateTime;
        TankStatuses::TankStatus tankStatus;
    };

public:
    Sync(const Sync&) = delete;
    Sync& operator =(const Sync&) = delete;
    Sync(const Sync&&) = delete;
    Sync& operator =(const Sync&&) = delete;

    explicit Sync(QObject *parent = nullptr);
    ~Sync();

public slots:
    void start();
    void stop();
    void addStatusForSync(const LevelGaugeService::TankID& id,const QDateTime dateTime, const TankStatuses::TankStatus& tankStatus);

private slots:
    void sync();

signals:
    void errorOccurred(const QString& msg) const;
    void finished() const;

private:
    void saveToDB(const StatusData& statusData, const TankID& id, const TankConfig& tankConfig, quint64 recordID) const;
    void saveLastDateTime(const StatusData& statusData, const TankID& id, const TankConfig& tankConfig) const;
    void saveToDBNitOilDepot(const StatusData& statusData, const TankID& id, const TankConfig& tankConfig) const;
    void saveToDBNitAZS(const StatusData& statusData, const TankID& id, const TankConfig& tankConfig) const;
    quint64 getLastSavetId(const TankConfig& tankConfig) const;
    void executeDBQuery(QSqlDatabase& db, const QString& queryText) const;

private:
    const TConfig *_cnf = nullptr;

    QTimer *_timer = nullptr;

    mutable QSqlDatabase _db;      //база данных с исходными данными
    mutable QSqlDatabase _dbNit;   //база данных АО НИТ

    TanksConfig _tanksConfig;

    std::queue<StatusData> _statusesData;

}; //class Sync

} //namespace LevelGaugeService

#endif // SYNC_H
