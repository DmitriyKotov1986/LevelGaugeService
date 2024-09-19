///////////////////////////////////////////////////////////////////////////////
/// Класс ядра. Управляет созданием и удалением объект, обработкой и
///     сохранением логов и начальной загрузкой
///
/// (с) Dmitriy Kotov, 2024
///////////////////////////////////////////////////////////////////////////////
#pragma once

//STL
#include <memory>
#include <vector>

//QT
#include <QObject>
#include <QList>
#include <QThread>

//My
#include "Common/tdbloger.h"
#include "tconfig.h"
#include "tanks.h"
#include "sync.h"

namespace LevelGaugeService
{

////////////////////////////////////////////////////////////////////////////////
/// Класс ядра
///
class Core
    : public QObject
{
    Q_OBJECT

public:
    explicit Core(QObject *parent = nullptr);

    /*!
        Деструктор
    */
    ~Core();

    void start();
    void stop();

signals:
    void errorOccurred(Common::EXIT_CODE errorCode, const QString& errorString);

private slots:
    void errorOccurredTankConfig(Common::EXIT_CODE errorCode, const QString& errorString);
    void errorOccurredTanks(Common::EXIT_CODE errorCode, const QString& errorString);
    void errorOccurredSync(Common::EXIT_CODE errorCode, const QString& errorString);

private:
    Q_DISABLE_COPY_MOVE(Core)

    bool startTankConfig();
    bool startSync();
    bool startTanks();

private:
    TConfig* _cnf = nullptr;            ///< Глобальная конфигурация
    Common::TDBLoger* _loger = nullptr; ///< Глобальны логер

    std::unique_ptr<TanksConfig> _tanksConfig;  ///< конфигурация резервуаров
    std::unique_ptr<Tanks> _tanks; ///< Список резервуаров
    std::unique_ptr<Sync> _sync;

}; // class Core

} // namespace LevelGaugeService

