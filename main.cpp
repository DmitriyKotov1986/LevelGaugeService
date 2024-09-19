//Qt
#include <QCoreApplication>
#include <QDir>
#include <QTextStream>
#include <QFileInfo>
#include <string>

//My
#include "Common/common.h"
#include "tconfig.h"
#include "service.h"

//для запуска как консольное приложение запускать с параметром -e

using namespace LevelGaugeService;
using namespace Common;

int main(int argc, char *argv[])
{
    setlocale(LC_ALL, ""); //настраиваем локаль
    qInstallMessageHandler(messageOutput);

    QCoreApplication::setApplicationName("LevelGaugeService");
    QCoreApplication::setOrganizationName("OOO 'SA'");
    QCoreApplication::setApplicationVersion(QString("Version:0.2 Build: %1 %2").arg(__DATE__).arg(__TIME__));

    //Создаем парсер параметров командной строки
    if ((argc > 1) && (!std::strcmp(argv[1], std::string("--version").c_str())))
    {
        QTextStream outStream(stdout);
        outStream << QString("%1 %2\n").arg(QCoreApplication::applicationName()).arg(QCoreApplication::applicationVersion());

        return EXIT_CODE::OK;
    }

    const QString applicationDirName = QFileInfo(argv[0]).absolutePath();
    const QString configFileName = QString("%1/%2.ini").arg(applicationDirName).arg(QCoreApplication::applicationName());

    TConfig* cnf = nullptr;
    Common::TDBLoger* loger = nullptr;
    Service* service = nullptr;
    try
    {
        cnf = TConfig::config(configFileName);
        if (cnf->isError())
        {
            throw StartException(EXIT_CODE::LOAD_CONFIG_ERR, QString("Error load configuration: %1").arg(cnf->errorString()));
        }

        //настраиваем подключение БД логирования
        loger = Common::TDBLoger::DBLoger(cnf->dbConnectionInfo(), "LevelGaugeServiceLog", cnf->sys_DebugMode());

        //создаем и запускаем сервис
        service = new Service(argc, argv);
        if (service->isError())
        {
            throw StartException(EXIT_CODE::SERVICE_INIT_ERR, QString("Service initialization error: %1").arg(service->errorString()));
        }

        loger->start();
        if (loger->isError())
        {
            throw StartException(EXIT_CODE::START_LOGGER_ERR, QString("Loger initialization error. Error: %1").arg(loger->errorString()));
        }
    }

    catch (const StartException& err)
    {
        delete service;
        TDBLoger::deleteDBLoger();
        TConfig::deleteConfig();

        qCritical() << err.what();

        return err.exitCode();
    }

    const auto res = service->exec();

    delete service;
    TDBLoger::deleteDBLoger();
    TConfig::deleteConfig();

    return res;
}

