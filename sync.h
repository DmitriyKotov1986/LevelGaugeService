///////////////////////////////////////////////////////////////////////////////
/// Основные классы для работы синхронизация с внешними БД и Серверами
///
/// (с) Dmitriy Kotov, 2024
///////////////////////////////////////////////////////////////////////////////
#pragma once

#include <QObject>

//My
#include "Common/common.h"
#include "Common/tdbloger.h"
#include "tanksconfig.h"
#include "tankstatuses.h"
#include "intake.h"

namespace LevelGaugeService
{

///////////////////////////////////////////////////////////////////////////////
///  Интерфейсный класс синхронизаторов
///
class SyncImpl
    : public QObject
{
    Q_OBJECT

public:
    /*!
        Конструктор
        @param parent - указатель на родительский класс
     */

    explicit SyncImpl(QObject* parent = nullptr);

    /*!
        Деструктор
    */
    ~SyncImpl();

    /*!
        Отправляет данные о новых статусах на сервер
        @param packetId - ИД пакета
        @param tankStatuses - список статусов. Список должен быть не пустой
    */
    virtual void calculateStatuses(const LevelGaugeService::TankID& id, const LevelGaugeService::TankStatusesList& tankStatuses);

    /*!
        Отправляет данные о новых приходах топлива на сервер
        @param packetId - ИД пакета
        @param tankStatuses - список данных о новых приходах. Список должен быть не пустой
    */
    virtual void calculateIntakes(const LevelGaugeService::TankID& id, const LevelGaugeService::IntakesList& intakes);

    /*!
        Вызывается при старте синхронизатора. Гарантируется что к моменту вызова сигналы  errorOccurred(...)
            и sendLogMsg(...) уже будут подключены к соотвестующим слотам.
    */
    virtual void start();

    /*!
        Вызывается при остановке синхронизатора
    */
    virtual void stop();

signals:
    /*!
        Сигнал испускаться в случае фатальной ошибки и невозможности дальнейшей работы
        @param syncName - символьное название сихронизатора
        @param errorCode - код ошибки
        @param errorString - текстовое описание ошибки
    */
    void errorOccurred(const QString& syncName, Common::EXIT_CODE errorCode, const QString& errorString);

    /*!
        Сигнал испускатся при необходимости сохранить сообщения в лог
        @param syncName - символьное название сихронизатора
        @param category - тип сохраняемого сообщения (DBG, INFO, WAR....)
        @param msg - текст сообщения
    */
    void sendLogMsg(const QString& syncName, Common::TDBLoger::MSG_CODE category, const QString &msg);

}; //class SyncImpl

///////////////////////////////////////////////////////////////////////////////
///  Класс управления синхронихронизацией
///
class Sync
    : public QObject
{
    Q_OBJECT

public:
    /*!
        Конструктор
        @param dbConnectionInfo - ссылка на информацию о подключении к БД
        @param tankConfig - ссылка на конфигурацию резервуара
        @param parent - указатель на родительский класс
    */
    explicit Sync(const Common::DBConnectionInfo& dbConnectionInfo, LevelGaugeService::TanksConfig* tanksConfig, QObject* parent = nullptr);

    /*!
        Деструктор
    */
    ~Sync();

public slots:
    /*!
        Поступили новые вычесленные статусы резервуаров.
        @param tankStatuses - список новых статусов. Список должен быть не пустым
    */
    void calculateStatuses(const LevelGaugeService::TankID& id, const LevelGaugeService::TankStatusesList& tankStatuses);

    /*!
        Поступили новые приходы
        @param intakes - список новых приходов. Список должен быть не пустым
    */
    void calculateIntakes(const LevelGaugeService::TankID& id, const LevelGaugeService::IntakesList& intakes);

    /*!
        Запуск всех синхронизаторов. Перед вызовом этого метода все сигналы/слоты должны быть подключены
    */
    void start();
    /*!
        Остановка всех синхронизаторов
    */
    void stop();

private slots:
    /*!
        Слот обработки сигналов errorOccurred(...) от синхронизаторов
    */
    void errorOccurredSync(const QString& syncName, Common::EXIT_CODE errorCode, const QString& errorString);

    /*!
        Слот обработки сигналов sendLogMsg(...) от синхронизаторов
    */
    void sendLogMsgSync(const QString& syncName, Common::TDBLoger::MSG_CODE category, const QString &msg);

signals:
    /*!
        Сигнал испускаться в случае фатальной ошибки и невозможности дальнейшей работы
        @param syncName - символьное название сихронизатора
        @param errorCode - код ошибки
        @param errorString - текстовое описание ошибки
    */
    void errorOccurred(Common::EXIT_CODE errorCode, const QString& errorString);

    /*!
        Сигнал испускатся при необходимости сохранить сообщения в лог
        @param category - тип сохраняемого сообщения (DBG, INFO, WAR....)
        @param msg - текст сообщения
    */
    void sendLogMsg(Common::TDBLoger::MSG_CODE category, const QString &msg);

    void finished();
    void started();

private:
    std::list<std::unique_ptr<SyncImpl>> _syncList;  ///< Список указателей на синхронизаторы
    bool _isStarted = false;  ///< Флаг работы синхонизаторов (==true между вызовами start() и stop()

}; //class Sync

} //namespace LevelGaugeService


