#ifndef SERVICE_H
#define SERVICE_H


#include <QString>

//My
#include <QtService/qtservice.h>
#include "Common/tdbloger.h"
#include "tconfig.h"
#include "core.h"

namespace LevelGaugeService
{

class Service : public QtService::QtService<QCoreApplication>
{
public:
   explicit Service(int argc, char **argv);
   ~Service();

    QString errorString();
    bool isError() const { return !_errorString.isEmpty(); }

protected:
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

#endif // SERVICE_H
