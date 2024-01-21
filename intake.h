#ifndef INTAKE_H
#define INTAKE_H

//QT
#include <QObject>
#include <QTimer>
#include <QHash>
#include <QString>

//My
#include "commondefines.h"
#include "tconfig.h"
#include "tankstatuses.h"

namespace LevelGaugeService
{

class Intake final
    : public QObject
{
    Q_OBJECT

public:
    Intake(const Intake&) = delete;
    Intake& operator =(const Intake&) = delete;
    Intake(const Intake&&) = delete;
    Intake& operator =(const Intake&&) = delete;

    explicit Intake(const TankStatuses& tankStatuses, QObject *parent = nullptr);
    ~Intake();

public:
    void start();
    void stop();
    void calculateIntake();

private:
    void saveIntake();          //Находит и сохраняет приходы
    TankStatuses::TankStatusesIterator getStartIntake();    //возвращает итератор на начало приема топлива
    TankStatuses::TankStatusesIterator getFinishedIntake(); //возвращает итератор на конец приема топлива

private:
    struct TankConfig
    {
        TankID id;
        QDateTime lastIntake;
        QString dbNitName;
        float deltaIntake = 0.0;
    };

private:
    TConfig *_cnf = nullptr;

    mutable QSqlDatabase _dbNit;   //база данных АО НИТ

    TankStatuses _tankStatuses;
    TankConfig _tankConfig;

}; //class Intake

} //namespace LevelGaugeService

#endif // INTAKE_H
