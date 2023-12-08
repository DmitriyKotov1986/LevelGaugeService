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
using namespace QtService;

int main(int argc, char *argv[])
{
    QCoreApplication::setApplicationName("LevelGaugeService");
    QCoreApplication::setOrganizationName("OOO 'SA'");
    QCoreApplication::setApplicationVersion(QString("Version:0.2 Build: %1 %2").arg(__DATE__).arg(__TIME__));

    setlocale(LC_CTYPE, ""); //настраиваем локаль

    //Создаем парсер параметров командной строки
    if ((argc > 1) && (!std::strcmp(argv[1], std::string("--version").c_str())))
    {
        QTextStream outStream(stdout);
        outStream << QCoreApplication::applicationName() << " " << QCoreApplication::applicationVersion() << "\n";

        return EXIT_CODE::OK;
    }

    Common::exitIfAlreadyRun();

    const QString applicationDirName = QFileInfo(argv[0]).absolutePath();
    const QString configFileName = QString("%1/%2.ini").arg(applicationDirName).arg(QCoreApplication::applicationName());

    writeLogFile("INF>", "Log file initialization. " + QCoreApplication::applicationVersion());

    auto cnf = TConfig::config(configFileName);

    if (cnf->isError())
    {
        QString msg = "Error load configuration: " + cnf->errorString();
        qCritical() << QString("%1 %2").arg(QTime::currentTime().toString("hh:mm:ss")).arg(msg);
        Common::writeLogFile("ERR>", msg);

        return EXIT_CODE::LOAD_CONFIG_ERR; // -1
    }

    //создаем и запускаем сервис
    Service service(argc, argv);

    if (service.isError())
    {
        QString msg = "Service initialization error: " + service.errorString();
        qCritical() << QString("%1 %2").arg(QTime::currentTime().toString("hh:mm:ss")).arg(msg);
        Common::writeLogFile("ERR>", msg);

        return EXIT_CODE::SERVICE_INIT_ERR; // -200
    }

    return service.exec();
}

