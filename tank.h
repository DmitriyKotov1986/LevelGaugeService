#pragma once

//STL
#include <map>
#include <memory>
#include <optional>

//QT
#include <QObject>
#include <QDateTime>
#include <QSqlDatabase>
#include <QTimer>
#include <QRandomGenerator>
#include <QPair>
#include <QHash>
#include <QThread>

//My
#include "Common/common.h"
#include "Common/tdbloger.h"
#include "intake.h"
#include "tankstatuses.h"
#include "tankconfig.h"

namespace LevelGaugeService
{

class Tank
    : public QObject
{
    Q_OBJECT

public:   
    /*!
        Конструктор. Планируеться использовать только такой конструктор
        @param dbConnectionInfo - параметры подключения к БД
        @param tankConfig - конфигурация резервуара
        @param tankSavedStatuse - сохраненные, но не обработанные статусы
        @param parent - указатаель на родительский класс
    */
    Tank(const LevelGaugeService::TankConfig* tankConfig, const TankStatusesList& tankSavedStatuses, QObject* parent = nullptr);
    ~Tank();

public slots:
    void start();
    void stop();

    /*!
        Получает новые статусы резервуара из таблицы измерений
        @param id - ИД резервуара
        @param tankStatuses - список новых статусов
    */
    void newStatuses(const LevelGaugeService::TankID& id, const LevelGaugeService::TankStatusesList& tankStatuses);

private slots:
    void addStatusEnd();
    void sendNewStatusesToSave();

    /*!
        Фатальная ошибка синхронизации
        @param code - код ошибки
        @param msg - текстовое описание
    */
    void errorOccurredSync(Common::EXIT_CODE code , const QString& msg);

    /*!
        Передача сообщения в лог от синхронизатора
        @param code - код ошибки
        @param msg - текстовое описание
    */
    void sendLogMsgSync(Common::TDBLoger::MSG_CODE code, const QString& msg);

signals:
    void calculateStatuses(const LevelGaugeService::TankID& id, const TankStatusesList &tankStatuses);
    void calculateIntakes(const LevelGaugeService::TankID& id, const IntakesList &intakes);

    /*!
        Фатальная ошибка обработки данных резервуаров
        @param id - ИД резервуара
        @param errorCode - код ошибки
        @param msg - текстовое описание
    */
    void errorOccurred(const LevelGaugeService::TankID& id, Common::EXIT_CODE errorCode, const QString& msg);


    void sendLogMsg(const LevelGaugeService::TankID& id, Common::TDBLoger::MSG_CODE category, const QString &msg);


    void finished();
    void started(const LevelGaugeService::TankID& id);

private:
    using TankStatusesIterator =  LevelGaugeService::TankStatuses::iterator;

private:
    // Удаляем неиспользуемые конструкторы
    Tank() = delete;
    Q_DISABLE_COPY_MOVE(Tank)

    void addStatuses(const LevelGaugeService::TankStatusesList& tankStatuses);

    void addStatus(const LevelGaugeService::TankStatus& tankStatus);

    void addStatusesRange(const LevelGaugeService::TankStatus& tankStatus);
    void addStatusesIntake(const LevelGaugeService::TankStatus& tankStatus);

    void addRandom(LevelGaugeService::TankStatus* tankStatus) const;
    void checkLimits(LevelGaugeService::TankStatus* tankStatus) const; //провеверяет лимитные ограничения статусов

    void clearTankStatuses();

    TankStatusesIterator getStartIntake();
    TankStatusesIterator getFinishedIntake();
    void findIntake();

    TankStatusesIterator getStartPumpingOut();
    TankStatusesIterator getFinishedPumpingOut();
    void findPumpingOut();

private:
    const LevelGaugeService::TankConfig* _tankConfig; //Конфигурация резервуар

    QRandomGenerator* _rg = nullptr;  //генератор случайных чисел для имитации разброса параметров при измерении в случае подстановки

    LevelGaugeService::TankStatuses _tankStatuses;
    QDateTime _lastSendToSaveDateTime;
    QDateTime _lastPumpingOut;

    std::optional<QDateTime> _isIntake;
    std::optional<QDateTime> _isPumpingOut;

    QTimer* _saveToDBTimer = nullptr;

     bool _isStarted = false;

};

} //namespace LevelGaugeService
