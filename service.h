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
    Service() = delete;
    Service(const Service&) = delete;
    Service& operator=(const Service&) = delete;
    Service(Service&&) = delete;
    Service& operator=(Service&&) = delete;

    explicit Service(int argc, char **argv);
    ~Service();

    QString errorString();
    bool isError() const { return !_errorString.isEmpty(); }

public slots:
    void errorOccurredLoger(Common::EXIT_CODE errorCode, const QString &errorString);
    void errorOccurredCore(Common::EXIT_CODE errorCode, const QString &errorString);

protected:
    virtual void start() override;  //Запус сервиса
    virtual void pause() override;  //Установка сервиса на паузу
    virtual void resume() override; //Востановление сервиса после паузы
    virtual void stop() override;   //Остановка сервиса

private:
    bool _isRun = false;
    TConfig* _cnf = nullptr;
    Common::TDBLoger* _loger = nullptr;

    Core* _core = nullptr;

    QString _errorString;

}; // class Service

} // namespace LevelGaugeService

