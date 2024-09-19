#pragma once

//Qt
#include <QString>
#include <QObject>

//My
#include <QtService/QtService>
#include "Common/tdbloger.h"
#include "tconfig.h"
#include "core.h"

namespace LevelGaugeService
{

class Service final
    : public QObject
    , public QtService<QCoreApplication>
{
    Q_OBJECT

public:
    explicit Service(int& argc, char **argv);

    /*!
        Деструктор
    */
    ~Service();

    QString errorString();
    bool isError() const { return !_errorString.isEmpty(); }

public slots:
    void errorOccurredLoger(Common::EXIT_CODE errorCode, const QString &errorString);
    void errorOccurredCore(Common::EXIT_CODE errorCode, const QString &errorString);

private:
    Service() = delete;
    Q_DISABLE_COPY_MOVE(Service)

    void start() override;  //Запус сервиса
    void pause() override;  //Установка сервиса на паузу
    void resume() override; //Востановление сервиса после паузы
    void stop() override;   //Остановка сервиса

private:
    bool _isRun = false;
    TConfig* _cnf = nullptr;
    Common::TDBLoger* _loger = nullptr;

    Core* _core = nullptr;

    QString _errorString;

}; // class Service

} // namespace LevelGaugeService

