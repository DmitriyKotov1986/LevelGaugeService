#include <QtSql/QSqlQuery>
#include <QSqlError>
#include <QtDebug>
#include <QRandomGenerator>
#include <QFile>
#include "tlevelgauge.h"

TLevelGauge::TLevelGauge(QSettings & Config, QObject *parent)
    : QObject(parent)
    , Config(Config)
{
//
}

TLevelGauge::~TLevelGauge()
{
//
}

void TLevelGauge::SendLogMsg(uint16_t Category, const QString &Msg)
{
    qDebug() << "LOG:" << Msg;
    //сохраняем сообщение в очередь
    TLogMsg tmp;
    tmp.Category = Category;
    tmp.UID = "";
    tmp.Sender = "LevelGaugeService";
    tmp.Msg = Msg;
    tmp.Msg.replace("'", "''");
    tmp.DateTime = QDateTime::currentDateTime();
    //помещаем сообщение в очередь
    LogQueue.enqueue(tmp);
}

void TLevelGauge::LoadTanksInfo()
{
    qDebug() << "===LOAD AZS CONFIG=========================================================================";

    QSqlQuery Query(DB);
    Query.setForwardOnly(true);
    DB.transaction();
    //загружаем данные об АЗС
    QString QueryText = "SELECT [AZSCode], [LevelGaugeServiceDB], [TimeShift] "
                        "FROM [AZSInfo] "
                        "WHERE [LevelGaugeServiceEnabled] = 1"    ;
    if (!Query.exec(QueryText)) {
        qDebug() << "FAIL Cannot execute query. Error: " << Query.lastError().text() << " Query: "<< Query.lastQuery();
        DB.rollback();
        exit(-1);
    }

    while (Query.next()) {
        TAZS tmp;
        tmp.DBName = Query.value("LevelGaugeServiceDB").toString();//имя БД для сохранения результатов
        tmp.TimeShift = Query.value("TimeShift").toInt(); //cмещение времени относительно сервера
        AZSS.insert(Query.value("AZSCode").toString(), tmp);
    }

    //загружаем корнфигурацию резервуаров
    QueryText = "SELECT [AZSCode], [TankNumber], [Volume], [Diametr], [Product], [LastSaveDateTime], [LastIntakeDateTime] "
                "FROM [TanksInfo] "
                "WHERE [AZSCode] IN (SELECT [AZSCode] FROM [AZSInfo] WHERE [LevelGaugeServiceEnabled] = 1) AND [Enabled] = 1 ";

    if (!Query.exec(QueryText)) {
        qDebug() << "FAIL Cannot execute query. Error: " << Query.lastError().text() << " Query: "<< Query.lastQuery();
        DB.rollback();
        exit(-1);
    }

    while (Query.next()) {
        TTankInfo &tmp = AZSS[Query.value("AZSCode").toString()].Tanks[Query.value("TankNumber").toUInt()].TanksInfo;
        tmp.Diametr = Query.value("Diametr").toFloat();
        tmp.Volume = Query.value("Volume").toFloat();
        tmp.Product = Query.value("Product").toString();
        tmp.LastSaveDateTime = Query.value("LastSaveDateTime").toDateTime();
        tmp.LastIntakeDateTime = Query.value("LastIntakeDateTime").toDateTime();
    }

 /*   //загружаем калибровочные таблицы
    QueryText = "SELECT [Volume], [Height], [Temp], a.[AZSCode], a.[TankNumber] "
                "FROM  [TanksCalibration]  as a "
                "INNER JOIN (SELECT [AZSCode], [TankNumber] "
                            "FROM [TanksInfo] "
                            "WHERE [AZSCode] IN (SELECT [AZSCode] "
                                                "FROM [AZSInfo] "
                                                "WHERE [LevelGaugeServiceEnabled] = 1) "
                                "AND [Enabled] = 1 ) AS b "
                "ON (a.AZSCode = b.AZSCode AND a.TankNumber = b.TankNumber)";

    if (!Query.exec(QueryText)) {
        qDebug() << "FAIL Cannot execute query. Error: " << Query.lastError().text() << " Query: "<< Query.lastQuery();
        DB.rollback();
        exit(-1);
    }

    while (Query.next()) {
        TTankInfo &CurrentTankInfo = AZSS[Query.value("AZSCode").toString()].Tanks[Query.value("TankNumber").toUInt()].TanksInfo;
        CurrentTankInfo.Calibration.insert(Query.value("Height").toFloat(), Query.value("Volume").toFloat());
    }*/

    //проверяем правильность калибровочных таблиц
/*    for (const auto& AZSItem : AZSS.keys()) {
        for (const auto& TankNumberItem : AZSS[AZSItem].Tanks.keys()) {
            const auto &CurrentTankCalibration = AZSS[AZSItem].Tanks[TankNumberItem].TanksInfo.Calibration;
            const auto &CurrentTankInfo = AZSS[AZSItem].Tanks[TankNumberItem].TanksInfo;

            if (CurrentTankCalibration.size() < 2) {
                SendLogMsg(MSG_CODE::CODE_ERROR, "Too few entries in the calibration table. AZS:" + AZSItem +
                                                 " TankNumber:" + QString::number(TankNumberItem));
                exit(-3);
            }
            if ((CurrentTankCalibration.find(0) == CurrentTankCalibration.end()) ||
                    ((CurrentTankCalibration.find(CurrentTankInfo.Diametr) == CurrentTankCalibration.end()))) {
                SendLogMsg(MSG_CODE::CODE_ERROR, "Incorrect format of the calibration table. Extreme values ​​are missing. AZS:" +
                                                  AZSItem + " TankNumber:" + QString::number(TankNumberItem));
                exit(-4);
            }

            if ((CurrentTankCalibration.firstKey() < 0) && (CurrentTankCalibration.first() < 0)) {
                SendLogMsg(MSG_CODE::CODE_ERROR, "Negative value into calibration table. AZS:" +
                                                  AZSItem + " TankNumber:" + QString::number(TankNumberItem));
                exit(-6);
            }

            float CurrentVolume = -1.0;
            for (const auto& HeightItem: CurrentTankCalibration.keys()) {
                //qDebug() << CurrentVolume << " " << HeightItem << " " << CurrentTankCalibration[HeightItem];
                if (CurrentVolume > CurrentTankCalibration[HeightItem]) {
                    SendLogMsg(MSG_CODE::CODE_ERROR, "Incorrect order of measurement points. AZS:" + AZSItem +
                                                     " TankNumber:" + QString::number(TankNumberItem) + " Height:" + QString::number(HeightItem) +
                                                     " Volume:" + QString::number(CurrentTankCalibration[HeightItem]));
                   // exit(-5);
                }
                CurrentVolume = CurrentTankCalibration[HeightItem];
            }


        }
    }*/

    //for (float height = 0; height < 3000; ++height) {
    //    qDebug() << height << " " << GetValueFromCalibration(AZSS["111"].Tanks[1], height);
    //}
    //загружаем данные о ТО
    QueryText = "SELECT [Start], [Finished], a.[AZSCode], a.[TankNumber] "
                "FROM [AZSService] as a "
                "INNER JOIN (SELECT [AZSCode], [TankNumber] "
                            "FROM [TanksInfo] "
                            "WHERE [AZSCode] IN (SELECT [AZSCode] "
                                                "FROM [AZSInfo] "
                                                "WHERE [LevelGaugeServiceEnabled] = 1) "
                                "AND [Enabled] = 1 ) AS b "
                "ON (a.AZSCode = b.AZSCode AND a.TankNumber = b.TankNumber)";

    if (!Query.exec(QueryText)) {
        qDebug() << "FAIL Cannot execute query. Error: " << Query.lastError().text() << " Query: "<< Query.lastQuery();
        DB.rollback();
        exit(-1);
    }

    while (Query.next()) {
        TTankInfo &CurrentTankInfo = AZSS[Query.value("AZSCode").toString()].Tanks[Query.value("TankNumber").toUInt()].TanksInfo;

        if (!Query.value("Finished").isNull()) {
            CurrentTankInfo.ServicePeriod.insert(Query.value("Finished").toDateTime(), Query.value("Start").toDateTime());
        }
        else {
            CurrentTankInfo.ServicePeriod.insert(QDateTime::currentDateTime().addYears(50), Query.value("Start").toDateTime());
        }
    }

    //Завершаем транкзацию
    if (!DB.commit()) {
        qDebug() << "FAIL Cannot commit transation. Error: " << DB.lastError().text();
        DB.rollback();
        exit(-2);
    };

    //выводим краткий отчет
    for (const auto &[AZSItem, AZSInfo] : AZSS.toStdMap()) {
        qDebug() << "Load info about AZS: " << AZSItem << "DB to save:" << AZSInfo.DBName << "Time shift:" << AZSInfo.TimeShift << "sec";
        for (const auto &[TankNumber, Tank] : AZSInfo.Tanks.toStdMap()) {
            qDebug() << "-->Tank added:" + QString::number(TankNumber) <</* "Calibration table record:" << Tank.TanksInfo.Calibration.size() <<*/
                        "Service period count:" << Tank.TanksInfo.ServicePeriod.size() << "Product:" << Tank.TanksInfo.Product << "Diametr:" << Tank.TanksInfo.Diametr;
        }
    }
}



void TLevelGauge::LoadMeasuments() //Загружаем фактические измерения
{
    qDebug() << "===LOAD MEASUMENTS=========================================================================";

    //выбираем данные для обработки
   // QList<TNewCalibration> NewCalibration; //сюда сохраняем новые точки калибровки длоя последующего сохранения в БД

    QSqlQuery Query(DB);
    Query.setForwardOnly(true);
    DB.transaction();

    QString QueryText;
    //выполняем запрос при запуске
    if (LastLoadID == 0) {
        //находим самое раннее время с которого надо запрашивать данные
        QDateTime LastDateTime = QDateTime::currentDateTime();
        for (const auto &AZSCode : AZSS.keys()) {
            for (const auto& TankNumber : AZSS[AZSCode].Tanks.keys()) {
                const TTank& CurrentTank = AZSS[AZSCode].Tanks[TankNumber];
                LastDateTime = std::min(CurrentTank.TanksInfo.LastSaveDateTime, LastDateTime);
            }
        }
        QueryText = "SELECT  a.[AZSCode], a.[TankNumber], [ID], [DateTime], [Density], [Height], [Volume], [Mass], [Temp] "
                    "FROM  [TanksMeasument]  as a "
                    "INNER JOIN (SELECT [AZSCode], [TankNumber] "
                                "FROM [TanksInfo] "
                                "WHERE [AZSCode] IN (SELECT [AZSCode] FROM [AZSInfo] WHERE [LevelGaugeServiceEnabled] = 1) AND [Enabled] = 1 ) AS b "
                    "ON (a.AZSCode = b.AZSCode AND a.TankNumber = b.TankNumber) "
                    "WHERE a.[DateTime] >= CAST('" + LastDateTime.toString("yyyy-MM-dd hh:mm:ss.zzz") + "' AS DATETIME2)";

    }
    else {
        QueryText = "SELECT  a.[AZSCode], a.[TankNumber], [ID], [DateTime], [Density], [Height], [Volume], [Mass], [Temp] "
                    "FROM  [TanksMeasument]  as a "
                    "INNER JOIN (SELECT [AZSCode], [TankNumber] "
                                "FROM [TanksInfo] "
                                "WHERE [AZSCode] IN (SELECT [AZSCode] FROM [AZSInfo] WHERE [LevelGaugeServiceEnabled] = 1) AND [Enabled] = 1 ) AS b "
                    "ON (a.AZSCode = b.AZSCode AND a.TankNumber = b.TankNumber) "
                    "WHERE ID > " + QString::number(LastLoadID);
    }

    //qDebug() << QueryText;
    if (!Query.exec(QueryText)) {
        qDebug() << "FAIL Cannot execute query. Error: " << Query.lastError().text() << " Query: "<< Query.lastQuery();
        DB.rollback();
        exit(-1);
    }

    while (Query.next()) {
       //qDebug() << Query.value("AZSCode").toString() << " " << Query.value("TankNumber").toUInt();
        TTank &CurrentTank = AZSS[Query.value("AZSCode").toString()].Tanks[Query.value("TankNumber").toUInt()];

        TStatus tmp;
        tmp.Density = Query.value("Density").toFloat();
        tmp.Height = Query.value("Height").toFloat();
        tmp.Temp = Query.value("Temp").toFloat();
        tmp.Volume = Query.value("Volume").toFloat();
        tmp.Mass = Query.value("Mass").toFloat();
        tmp.Flag = TAdditionFlag::MEASUMENTS;

        //запоминаем данные новее CurrentTank.TanksInfo.LastSaveDateTime
        QDateTime DateTime = Query.value("DateTime").toDateTime().addSecs(-AZSS[Query.value("AZSCode").toString()].TimeShift);
        if (DateTime > CurrentTank.TanksInfo.LastSaveDateTime) {
                CurrentTank.TankStatus.insert(Query.value("DateTime").toDateTime().addSecs(-AZSS[Query.value("AZSCode").toString()].TimeShift), tmp); //Добавляем значения. Минут т.к. в БД хранится в смещение часового пояса (с -)
        }

        //запоминаем данные новее имеющихся
        QDateTime MinDateTime = std::min(CurrentTank.TanksInfo.LastSaveDateTime, CurrentTank.TanksInfo.LastIntakeDateTime).addMSecs(-120);

        if (DateTime >= MinDateTime) {
            if (CurrentTank.TankStatus.isEmpty()) {
                CurrentTank.TankStatus.insert(Query.value("DateTime").toDateTime().addSecs(-AZSS[Query.value("AZSCode").toString()].TimeShift), tmp);
            }
            else if (DateTime < CurrentTank.TankStatus.firstKey()) {
                CurrentTank.TankStatus.insert(Query.value("DateTime").toDateTime().addSecs(-AZSS[Query.value("AZSCode").toString()].TimeShift), tmp); //Добавляем значения. Минут т.к. в БД хранится в смещение часового пояса (с -)
            }
        }
      /*  //Обновляем калибровочную таблицы
        if (CurrentTank.TanksInfo.Calibration.find(tmp.Height) == CurrentTank.TanksInfo.Calibration.end()) {
           // qDebug() << "Find new calibration value AZSCode:" << AZSCode << "TankNumber" << TankNumber << " ==> Height:" << tmp.Height << "Volume" << tmp.Volume;
            CurrentTank.TanksInfo.Calibration.insert(tmp.Height, tmp.Volume);
            TNewCalibration tmpNewCalibration;
            tmpNewCalibration.AZSCode = Query.value("AZSCode").toString();
            tmpNewCalibration.TankNumber = Query.value("TankNumber").toUInt();
            tmpNewCalibration.Height = tmp.Height;
            tmpNewCalibration.Volume = tmp.Volume;
            tmpNewCalibration.Temp = tmp.Temp;
            NewCalibration.push_back(tmpNewCalibration);

            CurrentTank.TanksInfo.Calibration.insert(tmp.Height, tmp.Volume); //добавляем в текущии калибровочные таблицы
        }*/
        LastLoadID = std::max(Query.value("ID").toULongLong(), LastLoadID);
    }

    if (!DB.commit()) {
        qDebug() << "FAIL Cannot commit transation. Error: " << DB.lastError().text();
        DB.rollback();
        exit(-2);
    };

    //AddNewCalibrationPoints(NewCalibration);

    for (const auto &[AZSItem, AZSInfo] : AZSS.toStdMap()) {
        qDebug() << "Measuments from AZS:" << AZSItem;
        for (const auto &[TankNumber, Tank] : AZSInfo.Tanks.toStdMap()) {
            if (!Tank.TankStatus.isEmpty()) {
                qDebug() << "-->Tank " + QString::number(TankNumber) << "Load time:" << Tank.TankStatus.firstKey().toString("yyyy-MM-dd hh:mm:ss") << "->" <<
                            Tank.TankStatus.lastKey().toString("yyyy-MM-dd hh:mm:ss")<< "Total Records load:" << Tank.TankStatus.size();
            }
            else {
                qDebug() << "-->Tank " + QString::number(TankNumber)  << "Not found record";
            }
        }
    }

    qDebug() << "Last record ID: " << LastLoadID;

   // exit(-100);
}

void TLevelGauge::LoadLastWriteData()
{


    qDebug() << "===LOAD LAST SAVE DATA=========================================================================";
    QSqlQuery Query(LGDB);
    Query.setForwardOnly(true);
    LGDB.transaction();

    //загружаем данные о последних сохраненных данных
    //Находим самую старую дату с которой нам нужно загружать данные
    //выполняем select в цикле для упрощения кода
    for (const auto &AZSCode : AZSS.keys()) {

        QDateTime LastDateTime = QDateTime::currentDateTime();
        QStringList TanksList;
        for (const auto& TankNumber : AZSS[AZSCode].Tanks.keys())  {
            const TTank& CurrentTank = AZSS[AZSCode].Tanks[TankNumber];
            LastDateTime = std::min(CurrentTank.TanksInfo.LastIntakeDateTime, LastDateTime);
            LastDateTime = std::min(CurrentTank.TanksInfo.LastSaveDateTime, LastDateTime);
            TanksList << QString::number(TankNumber);
        }

        QString QueryText = "SELECT [TankNumber], [DateTime], [Volume], [Mass], [Density], [Height], [Temp] "
                            "FROM [" + AZSS[AZSCode].DBName + "].[dbo].[TanksStatus] "
                            "WHERE [AZSCode] = '" + AZSCode + "' AND [TankNumber] IN (" + TanksList.join(",") + ") "
                                "AND [DateTime] >= CAST('" + LastDateTime.toString("yyyy-MM-dd hh:mm:ss.zzz") + "' AS DATETIME2)";

        if (!Query.exec(QueryText)) {
            qDebug() << "FAIL Cannot execute query. Error: " << Query.lastError().text() << " Query: "<< Query.lastQuery();
            LGDB.rollback();
            exit(-1);
        }

        //сохраняем
        while (Query.next()) {
            auto& CurrentTankStatus = AZSS[AZSCode].Tanks[Query.value("TankNumber").toUInt()].TankStatus;
            const auto& CurrentTankInfo = AZSS[AZSCode].Tanks[Query.value("TankNumber").toUInt()].TanksInfo;

            if (Query.value("DateTime").toDateTime().addSecs(-AZSS[AZSCode].TimeShift) >=
                std::min(CurrentTankInfo.LastIntakeDateTime, CurrentTankInfo.LastSaveDateTime)) {

                TStatus tmp;
                tmp.Density = Query.value("Density").toFloat();
                tmp.Height = Query.value("Height").toFloat() * 10; //переводим высоту обратно в мм
                tmp.Mass = Query.value("Mass").toFloat();
                tmp.Temp = Query.value("Temp").toFloat();
                tmp.Volume = Query.value("Volume").toFloat();
                tmp.Flag = TAdditionFlag::SAVE;

                CurrentTankStatus.insert(Query.value("DateTime").toDateTime().addSecs(-AZSS[AZSCode].TimeShift), tmp);
            }

        }

        qDebug() << "Last save data for AZS: " << AZSCode;
        for (const auto &TankNumber : AZSS[AZSCode].Tanks.keys())  {
            if (!AZSS[AZSCode].Tanks[TankNumber].TankStatus.isEmpty()) {
                auto& CurrentTank = AZSS[AZSCode].Tanks[TankNumber];
                const auto& LastSaveStatus = CurrentTank.TankStatus.last();
                qDebug() << "-->Tank:" << QString::number(TankNumber)  << "Load date:"<< AZSS[AZSCode].Tanks[TankNumber].TankStatus.firstKey().toString("yyyy-MM-dd hh:mm:ss.zzz") <<
                            "->" << AZSS[AZSCode].Tanks[TankNumber].TankStatus.lastKey().toString("yyyy-MM-dd hh:mm:ss.zzz") << "Last value:" <<
                            "Volume=" <<  LastSaveStatus.Volume << "Height=" << LastSaveStatus.Height << "Densyty:" <<  LastSaveStatus.Density
                            << "Mass=" <<  LastSaveStatus.Mass << "Temp=" <<  LastSaveStatus.Temp
                            << "Load records:" << AZSS[AZSCode].Tanks[TankNumber].TankStatus.size();
               //сохраняем последний записанный статус
               CurrentTank.TanksInfo.LastWriteStatus = LastSaveStatus;
            }
            else {
                qDebug() << "-->Tank:" << QString::number(TankNumber) << "Not found last save record";
            }
        }
    }

    if (!LGDB.commit()) {
        qDebug() << "FAIL Cannot commit transation. Error: " << LGDB.lastError().text();
        LGDB.rollback();
        exit(-2);
    }
}

void TLevelGauge::SaveMeasuments()
{
    qDebug() << "===SAVE DATA===================================================================================";

    QSqlQuery Query(LGDB); //LevelGauge DB
    QSqlQuery QueryDB(DB); //HTTPServer DB


    for (const auto &AZSCode : AZSS.keys()) {
        LGDB.transaction();
        DB.transaction();
        for (const auto &TankNumber : AZSS[AZSCode].Tanks.keys()) {          
            TTank &CurrentTank = AZSS[AZSCode].Tanks[TankNumber];
            qDebug() << "Save AZS: " << AZSCode << "Tank:" << TankNumber;
            if (CurrentTank.TankStatus.isEmpty()) {
                SendLogMsg(MSG_CODE::CODE_ERROR, "No data for save. AZS: " + AZSCode + " Tank: " + QString::number(TankNumber));
                continue;
            }

            //если дошли до сюда - то какие-то данные имеются
            for (auto Iter = GetRecordForSave(AZSCode, TankNumber); Iter != CurrentTank.TankStatus.end(); ++Iter) {
                //если сейчас не сервисный период - сохраняем значения
                if (!TestServiceIntervel(CurrentTank, Iter.key())) {
                    //qDebug() << std::distance(Iter,CurrentTank.TankStatus.end());
                    QString QueryText = "INSERT INTO [" + AZSS[AZSCode].DBName + "].[dbo].[TanksStatus] ([DateTime], [AZSCode], [TankNumber], [Product], [Height], [Volume], [Temp], [Density], [Mass]) VALUES ( "
                                       "CAST('" + Iter.key().addSecs(AZSS[AZSCode].TimeShift).toString("yyyy-MM-dd hh:mm:ss.zzz") + "' AS DATETIME2), " //добавляем смещение времени обратно
                                       "'" + AZSCode +  "', " +
                                       QString::number(TankNumber) +  ", "
                                       "'" + CurrentTank.TanksInfo.Product +"', " +
                                       QString::number(Iter->Height / 10.0, 'f', 1)  + ", " +
                                       QString::number(Iter->Volume, 'f', 0)  + ", " +
                                       QString::number(Iter->Temp, 'f', 1)  + ", " +
                                       QString::number(Iter->Density, 'f', 1)  + ", " +
                                       QString::number(Iter->Mass, 'f', 0)  + ")";
                     //qDebug() << QueryText;

                    if (!Query.exec(QueryText)) {
                        qDebug() << "FAIL Cannot execute query. Error: " << Query.lastError().text() << " Query: "<< Query.lastQuery();
                        LGDB.rollback();
                        DB.rollback();
                        exit(-1);
                    }
                }
                else {
                    qDebug() << "-->Skip. Service: " << Iter.key().toString("yyyy-MM-dd hh:mm:ss.zzz");
                }
            }

            //обновляем дату пoследей отправленной записи
            CurrentTank.TanksInfo.LastSaveDateTime = CurrentTank.TankStatus.lastKey();
            CurrentTank.TanksInfo.LastWriteStatus = CurrentTank.TankStatus.last(); //сохраняем последний сохранненое состояние
            qDebug() << "-->DateTime last save record:" << CurrentTank.TanksInfo.LastSaveDateTime.toString("yyyy-MM-dd hh:mm:ss.zzz");
            //если были новые данные - обновляем данные о последних загруженных сведениях
            //exit(0);
            QString QueryText = "UPDATE [TanksInfo] SET "
                                "[LastSaveDateTime] = CAST('" + CurrentTank.TanksInfo.LastSaveDateTime.toString("yyyy-MM-dd hh:mm:ss.zzz") + "' AS DATETIME2) "
                                "WHERE ([AZSCode] = '" + AZSCode + "') AND "
                                "([TankNumber] = " + QString::number(TankNumber) + ") ";

            if (!QueryDB.exec(QueryText)) {
                qDebug() << "FAIL Cannot execute query. Error: " << QueryDB.lastError().text() << " Query: "<< QueryDB.lastQuery();
                DB.rollback();
                LGDB.rollback();
                exit(-1);
            }
        }
        if (!DB.commit()) {
            qDebug() << "FAIL Cannot commit transation. Error: " << DB.lastError().text();
            LGDB.rollback();
            DB.rollback();
            return;
        };

        if (!LGDB.commit()) {
            qDebug() << "FAIL Cannot commit transation. Error: " << LGDB.lastError().text();
            LGDB.rollback();
            DB.rollback();
            return;
        };
    }
}

void TLevelGauge::AddStatus(TTank &CurrentTank, const QDateTime &Start, const QDateTime &End, const TStatus &StartAggregate, const TStatus &EndAggregate)
{
    qDebug() << "---->Add missing record: Date:" << Start.toString("yyyy-MM-dd hh:mm:ss.zzz") << "->" << End.toString("yyyy-MM-dd hh:mm:ss.zzz") <<
                "Height:" << StartAggregate.Height << "->" << EndAggregate.Height <<
                "Volume:" << StartAggregate.Volume << "->" << EndAggregate.Volume<<
                "Density:" << StartAggregate.Density << "->" << EndAggregate.Density <<
                "Mass:" << StartAggregate.Mass << "->" << EndAggregate.Mass <<
                "Temp:" << StartAggregate.Temp << "->" << EndAggregate.Temp;

    QDateTime i = Start;
    TStatus tmp = StartAggregate;
    tmp.Flag = TAdditionFlag::ADD;
    //вычисляем дельту
    TDelta Delta;
    float Count = (float)Start.secsTo(End) / 60.0; //количество шагов
    if (Count < 1) return; //выходим если шагов меньше 1

  //  tmp.Volume = GetValueFromCalibration(CurrentTank, tmp.Height) ;
  //  tmp.Mass = tmp.Volume * tmp.Density / 1000.0; // dev 1000 тк плотность в кг.м3, а объем в л

    Delta.Density = (EndAggregate.Density - StartAggregate.Density) / Count;
    Delta.Height = (EndAggregate.Height - StartAggregate.Height) / Count;
    Delta.Temp = (EndAggregate.Temp - StartAggregate.Temp) / Count;
    Delta.Volume = (EndAggregate.Volume - StartAggregate.Volume) / Count;


    while (i < End.addSecs(-30)) {
        CurrentTank.TankStatus[i] = tmp;

        //добавить рандомный +/-
        CurrentTank.TankStatus[i].Density += round(rg->bounded(-100, +100) / 195.0) * 0.1;
        CurrentTank.TankStatus[i].Height += round(rg->bounded(-100, +100) / 195.0);
        CurrentTank.TankStatus[i].Temp += round(rg->bounded(-100, +100) / 195.0) * 0.1;
   //     CurrentTank.TankStatus[i].Volume = GetValueFromCalibration(CurrentTank, CurrentTank.TankStatus[i].Height);
        CurrentTank.TankStatus[i].Volume += ((CurrentTank.TankStatus[i].Height - tmp.Height) > 0 ?  1.0 : - 1.0) * round(rg->bounded(0, 10));
        CurrentTank.TankStatus[i].Mass = CurrentTank.TankStatus[i].Volume * CurrentTank.TankStatus[i].Density / 1000.0; // dev 1000 тк плотность в кг.м3, а объем в л

 /*       qDebug() << "------>Add: Date:" << i.toString("yyyy-MM-dd hh:mm:ss.zzz") <<
                    "Height:" <<  CurrentTank.TankStatus[i].Height << "Volume:" <<  CurrentTank.TankStatus[i].Volume <<
                    "Density:" <<  CurrentTank.TankStatus[i].Density << "Mass:" <<  CurrentTank.TankStatus[i].Mass <<
                    "Temp:" <<  CurrentTank.TankStatus[i].Temp;*/

        //вычисляем смещение на следующий шаг
        i = i.addMSecs(60000 + rg->bounded(-1000, +1000));

        tmp.Density += Delta.Density;
        tmp.Height += Delta.Height;
        tmp.Temp += Delta.Temp ;
        tmp.Volume += Delta.Volume;
    }
}

void TLevelGauge::TestStatus(TTank &CurrentTank, TStatusIterator StartIter)
{
    qDebug() << "-->Checking records between:" << StartIter.key().toString("yyyy-MM-dd hh:mm:ss.zzz") << "and" << CurrentTank.TankStatus.lastKey().toString("yyyy-MM-dd hh:mm:ss.zzz");
    for (auto Iter = StartIter; Iter != std::prev(CurrentTank.TankStatus.end()); ++Iter) {
        auto NextIter = std::next(Iter); //итератор на следующую запись
        TDelta Delta; //разница между соседними элементами
        Delta.Density = NextIter->Density - Iter->Density;
        Delta.Height = NextIter->Height - Iter->Height;
        Delta.Temp = NextIter->Temp - Iter->Temp;
        Delta.Volume = NextIter->Volume - Iter->Volume;
        Delta.Mass = NextIter->Mass - Iter->Mass;

        if ((NextIter->Height > CurrentTank.TanksInfo.Diametr) || (NextIter->Height < 0)) {
            SendLogMsg(MSG_CODE::CODE_ERROR, "Exceeding the limit value Height. Test record: " + NextIter.key().toString("yyyy-MM-dd hh:mm:ss.zzz") +
                                             " Density=" + QString::number(NextIter.value().Density) + " Height=" +  QString::number(NextIter.value().Height) +
                                             " Volume=" + QString::number(NextIter.value().Volume) + " Mass=" +  QString::number(NextIter.value().Mass) +
                                             " Temp=" +  QString::number(NextIter.value().Temp) + " New value Height=" + QString::number(CurrentTank.TanksInfo.Diametr));
            NextIter->Height = CurrentTank.TanksInfo.Diametr;
            NextIter->Flag = TAdditionFlag::LIMIT;

        }

        if ((NextIter->Volume > CurrentTank.TanksInfo.Volume) || (NextIter->Volume < 0)) {
            SendLogMsg(MSG_CODE::CODE_ERROR, "Exceeding the limit value Volume. Test record: " + NextIter.key().toString("yyyy-MM-dd hh:mm:ss.zzz") +
                                             " Density=" + QString::number(NextIter.value().Density) + " Height=" +  QString::number(NextIter.value().Height) +
                                             " Volume=" + QString::number(NextIter.value().Volume) + " Mass=" +  QString::number(NextIter.value().Mass) +
                                             " Temp=" +  QString::number(NextIter.value().Temp) + " New value Volume=" + QString::number(CurrentTank.TanksInfo.Volume));
            NextIter->Volume = CurrentTank.TanksInfo.Volume;
            NextIter->Flag = TAdditionFlag::LIMIT;

        }


        if (Delta.Height <= 5.0) { //уровень убываетю небольшой плюс для компенсации случайных колебаний уровня
            TestStatusIter(Iter, Delta, MaxDelta);
        }
        else { //уровень растет
            TestStatusIter(Iter, Delta, MaxIntakeDelta);
        }


    }
}

void TLevelGauge::TestStatusIter(TStatusIterator Iter, const TDelta &Delta, const TDelta &MaxDelta)
{
    auto NextIter = std::next(Iter);
    if ((std::abs(Delta.Temp) - MaxDelta.Temp) > 0.001) {
        qDebug() << "---->Test record: " << NextIter.key().toString("yyyy-MM-dd hh:mm:ss.zzz") << "Density=" <<  NextIter.value().Density << "Height=" <<  NextIter.value().Height <<
                    "Volume=" <<  NextIter.value().Volume << "Mass=" <<  NextIter.value().Mass << "Temp=" <<  NextIter.value().Temp;
        NextIter->Temp = Iter->Temp + MaxDelta.Temp * (Delta.Temp >= 0.0 ? 1.0 : -1.0);
        NextIter->Flag = TAdditionFlag::TEST;
        qDebug() << "------>Too big delta. Temp. New value: " << NextIter->Temp;
    }

    bool ChangeDensity = false;
    if ((std::abs(Delta.Density) - MaxDelta.Density) > 0.001) {
        qDebug() << "---->Test record: " << NextIter.key().toString("yyyy-MM-dd hh:mm:ss.zzz") << "Density=" <<  NextIter.value().Density << "Height=" <<  NextIter.value().Height <<
                    "Volume=" <<  NextIter.value().Volume << "Mass=" <<  NextIter.value().Mass << "Temp=" <<  NextIter.value().Temp;
        ChangeDensity = true;
        NextIter->Density = Iter->Density + MaxDelta.Density * (Delta.Density >= 0.0 ? 1.0 : -1.0);
        NextIter->Flag = TAdditionFlag::TEST;
        qDebug() << "------>Too big delta. Density. New value: " << NextIter->Density;
    }

    bool ChangeHeight = false;
    if ((std::abs(Delta.Height) - MaxDelta.Height) > 0.01) {
        qDebug() << "---->Test record: " << NextIter.key().toString("yyyy-MM-dd hh:mm:ss.zzz") << "Density=" <<  NextIter.value().Density << "Height=" <<  NextIter.value().Height <<
                    "Volume=" <<  NextIter.value().Volume << "Mass=" <<  NextIter.value().Mass << "Temp=" <<  NextIter.value().Temp;
        ChangeHeight = true;
        NextIter->Height = Iter->Height + MaxDelta.Height * (Delta.Height >= 0.0 ? 1.0 : -1.0);
        NextIter->Flag = TAdditionFlag::TEST;
        qDebug() << "------>Too big delta. Height. New value: " << NextIter->Height;
    }

    //нам нужно пересчитать объем если: 1. изменилась высота 2.делта обьема слишком большая
    bool ChangeVolume = false;
   // Delta.Volume = GetValueFromCalibration(CurrentTank, NextIter->Height) - NextIter->Volume;
    if (ChangeHeight) {
        ChangeVolume = true;
        NextIter->Volume = Iter->Volume + MaxDelta.Volume * (Delta.Height >= 0.0 ? 0.1 : -0.1);
        qDebug() << "----->Volume recalculation. New value: " << NextIter->Volume;
    }
    else if ((std::abs(Delta.Volume) - MaxDelta.Volume) > 0.01) {
        qDebug() << "---->Test record: " << NextIter.key().toString("yyyy-MM-dd hh:mm:ss.zzz") << "Density=" <<  NextIter.value().Density << "Height=" <<  NextIter.value().Height <<
                    "Volume=" <<  NextIter.value().Volume << "Mass=" <<  NextIter.value().Mass << "Temp=" <<  NextIter.value().Temp;
        ChangeVolume = true;
        NextIter->Volume = Iter->Volume + MaxDelta.Volume * (Delta.Volume >= 0.0 ? 1.0 : -1.0);
        NextIter->Flag = TAdditionFlag::TEST;

        qDebug() << "------>Too big delta. Volume. New value: " << NextIter->Volume;
    }

    //пересчитываем массу если: 1. изменились обьем или плотность 2. дельта массы слишком большая
    if ((ChangeVolume) || (ChangeDensity)) {
        float NewDelteMass = NextIter->Mass - NextIter->Volume * NextIter->Density / 1000.0;
        if ((std::abs(NewDelteMass) - MaxDelta.Mass) > 0.01) NextIter->Mass = NextIter->Volume * NextIter->Density / 1000.0;
        else NextIter->Mass = Iter->Mass + MaxDelta.Mass * (NewDelteMass >= 0.0 ? 1.0 : -1.0);
        qDebug() << "------>Mass recalculation. New value: " << NextIter->Mass;
    }

    else if ((std::abs(Delta.Mass) - MaxDelta.Mass) > 0.01) {
        qDebug() << "---->Test record: " << NextIter.key().toString("yyyy-MM-dd hh:mm:ss.zzz") << "Density=" <<  NextIter.value().Density << "Height=" <<  NextIter.value().Height <<
                    "Volume=" <<  NextIter.value().Volume << "Mass=" <<  NextIter.value().Mass << "Temp=" <<  NextIter.value().Temp;
        NextIter->Mass = Iter->Mass + MaxDelta.Mass * (Delta.Mass >= 0.0 ? 1.0 : -1.0);
        NextIter->Flag = TAdditionFlag::TEST;

        qDebug() << "------>Too big delta. Mass. New value: " << NextIter->Mass;
    }
}

bool TLevelGauge::TestServiceIntervel(const TTank &CurrentTank, const QDateTime &DateTime)
{
    auto Iter = CurrentTank.TanksInfo.ServicePeriod.upperBound(DateTime);
    if (Iter != CurrentTank.TanksInfo.ServicePeriod.end()) {
        if (Iter.value() <= DateTime) return true;
    }
    return false;
}

void TLevelGauge::SaveIntake()
{
    qDebug() << "TEST INTAKE==============================================================================";

    for (const auto &AZSCode : AZSS.keys()) {
     //   qDebug() << "Save AZS: " << AZSCode;
        for (const auto &TankNumber : AZSS[AZSCode].Tanks.keys()) {
            TTank &CurrentTank = AZSS[AZSCode].Tanks[TankNumber];
            qDebug() << "Intake AZS:" << AZSCode << "TankNumber:" << TankNumber <<
                        "Last intake DateTime = " << CurrentTank.TanksInfo.LastIntakeDateTime.toString("yyyy-MM-dd hh:mm:ss.zzz");

            auto LastIntakeIter = CurrentTank.TankStatus.upperBound(CurrentTank.TanksInfo.LastIntakeDateTime); //ищем следующюю запись за последним сливом
            //если до конца меньше 30 записей - то пропускаем резервуар
            if (std::distance(LastIntakeIter, CurrentTank.TankStatus.end()) < 30) {
                 qDebug() << "-->Too little data to analyze. Skip";
                 continue;
            }

           // bool FindEndIntake = false; //флаг найденного начала слива
           // bool FindStartIntake = false;  //флаг найденного конца слива


            //ищем начало слива
            const auto StartIntakeIter = GetStartIntake(CurrentTank, std::next(LastIntakeIter, 5)); //смещаемся на 5 элементов вперед это нам гарантируем что в конце мы сможем взять нужные значения до

            if (StartIntakeIter != CurrentTank.TankStatus.end()) {

                //ищем конец слива
                const auto FinishedIntakeIter = GetFinishedIntake(CurrentTank, StartIntakeIter);

                if (FinishedIntakeIter != CurrentTank.TankStatus.end()) {
                    QSqlQuery Query(LGDB);
                    QSqlQuery QueryDB(DB);
                    LGDB.transaction();
                    DB.transaction();

                    const auto FirstIter = std::prev(StartIntakeIter, 5);
                    const auto LastIter = std::next(FinishedIntakeIter, 5);

                    qDebug() << "-->Find intake beetwen: " << FirstIter.key().toString("yyyy-MM-dd hh:mm:ss.zzz") <<
                                "and" << LastIter.key().toString("yyyy-MM-dd hh:mm:ss.zzz") <<
                                "Delta mass:" << (LastIter->Mass - FirstIter->Mass);
                    //если найденный слив не входит в сервисные интервалы - то записываем его в таблицу
                    if (!TestServiceIntervel(CurrentTank, FirstIter.key()) && (!TestServiceIntervel(CurrentTank, LastIter.key()))) {
                        //сохраняем приход в БД уровнемеров
                        QString QueryText = "INSERT INTO [" + AZSS[AZSCode].DBName + "].[dbo].[AddProduct] "
                                            "([DateTime], [AZSCode], [TankNumber], [Product], [StartDateTime], [FinishedDateTime], [Height], [Volume], [Temp], [Density], [Mass])"
                                            "VALUES ( "
                                            "CAST('" + QDateTime::currentDateTime().addSecs(AZSS[AZSCode].TimeShift).toString("yyyy-MM-dd hh:mm:ss.zzz") + "' AS DATETIME2), "
                                            "'" + AZSCode +  "', " +
                                            QString::number(TankNumber) +  ", "
                                            "'" + CurrentTank.TanksInfo.Product +"', " +
                                            "CAST('" + FirstIter.key().addSecs(AZSS[AZSCode].TimeShift).toString("yyyy-MM-dd hh:mm:ss.zzz") + "' AS DATETIME2), "
                                            "CAST('" + LastIter.key().addSecs(AZSS[AZSCode].TimeShift).toString("yyyy-MM-dd hh:mm:ss.zzz") + "' AS DATETIME2), " +
                                            QString::number(LastIter->Height / 10.0, 'f', 1)  + ", " +
                                            QString::number(LastIter->Volume, 'f', 0)  + ", " +
                                            QString::number(LastIter->Temp, 'f', 1)  + ", " +
                                            QString::number(LastIter->Density, 'f', 1)  + ", " +
                                            QString::number(LastIter->Mass - FirstIter->Mass, 'f', 0)  + ")";
                        // qDebug() << QueryText;
                        if (!Query.exec(QueryText)) {
                            qDebug() << "FAIL Cannot execute query. Error: " << Query.lastError().text() << " Query: "<< Query.lastQuery();
                            DB.rollback();
                            LGDB.rollback();
                            exit(-1);
                        }
                        //вставляем в нашу таблицу
                        QueryText = "INSERT INTO [TanksIntake] "
                                    "([DateTime], [AZSCode], [TankNumber], [Product], [StartDateTime], [FinishedDateTime], [Height], [Volume], [Temp], [Density], [Mass])"
                                    "VALUES ( "
                                    "CAST('" + QDateTime::currentDateTime().addSecs(AZSS[AZSCode].TimeShift).toString("yyyy-MM-dd hh:mm:ss.zzz") + "' AS DATETIME2), "
                                    "'" + AZSCode +  "', " +
                                    QString::number(TankNumber) +  ", "
                                    "'" + CurrentTank.TanksInfo.Product +"', " +
                                    "CAST('" + FirstIter.key().addSecs(AZSS[AZSCode].TimeShift).toString("yyyy-MM-dd hh:mm:ss.zzz") + "' AS DATETIME2), "
                                    "CAST('" + LastIter.key().addSecs(AZSS[AZSCode].TimeShift).toString("yyyy-MM-dd hh:mm:ss.zzz") + "' AS DATETIME2), " +
                                    QString::number(LastIter->Height / 10.0, 'f', 1)  + ", " +
                                    QString::number(LastIter->Volume, 'f', 0)  + ", " +
                                    QString::number(LastIter->Temp, 'f', 1)  + ", " +
                                    QString::number(LastIter->Density, 'f', 1)  + ", " +
                                    QString::number(LastIter->Mass - FirstIter->Mass, 'f', 0)  + ")";
                        // qDebug() << QueryText;
                        if (!QueryDB.exec(QueryText)) {
                            qDebug() << "FAIL Cannot execute query. Error: " << QueryDB.lastError().text() << " Query: "<< QueryDB.lastQuery();
                            DB.rollback();
                            LGDB.rollback();
                            exit(-1);
                        }
                        //обновляем информацию о найденном сливае
                        QueryText = "UPDATE [TanksInfo] SET "
                                    "[LastIntakeDateTime] = CAST('" + LastIter.key().toString("yyyy-MM-dd hh:mm:ss.zzz") + "' AS DATETIME2) "
                                    "WHERE ([AZSCode] = '" + AZSCode + "') AND "
                                    "([TankNumber] = " + QString::number(TankNumber) + ") ";

                        if (!QueryDB.exec(QueryText)) {
                            qDebug() << "FAIL Cannot execute query. Error: " << QueryDB.lastError().text() << " Query: "<< QueryDB.lastQuery();
                            DB.rollback();
                            LGDB.rollback();
                            exit(-1);
                        }
                    }
                    //обновляем конец прихода
                    CurrentTank.TanksInfo.LastIntakeDateTime = LastIter.key();

                    if (!DB.commit()) {
                        qDebug() << "FAIL Cannot commit transation. Error: " << DB.lastError().text();
                        LGDB.rollback();
                        DB.rollback();
                        exit(-2);
                    }

                    if (!LGDB.commit()) {
                        qDebug() << "FAIL Cannot commit transation. Error: " << LGDB.lastError().text();
                        LGDB.rollback();
                        DB.rollback();
                        exit(-2);
                    }
                }
            }
       }
   }

}

TLevelGauge::TStatusConstIterator TLevelGauge::GetStartIntake(const TTank &CurrentTank, TStatusConstIterator StartFindIter)
{
    for (auto FirstIter = StartFindIter; FirstIter != std::prev(CurrentTank.TankStatus.end(), 6);  ++FirstIter) {
     //   qDebug() << "------>Date:" << FirstIter.key().toString("yyyy-MM-dd hh:mm:ss.zzz") <<
     //                       "Height:" <<  FirstIter->Height << "Volume:" <<  FirstIter->Volume <<
     //                       "Density:" <<  FirstIter->Density << "Mass:" <<  FirstIter->Mass <<
     //                       "Temp:" <<  FirstIter->Temp;
        const auto SecondIter = std::next(FirstIter, 5);
        if (SecondIter.value().Volume - FirstIter->Volume >= DeltaIntakeVolume) {
            qDebug() << "-->Find start intake: " << FirstIter.key().toString("yyyy-MM-dd hh:mm:ss.zzz");
            return FirstIter;
        }
    }
    return CurrentTank.TankStatus.end();
}

TLevelGauge::TStatusConstIterator TLevelGauge::GetFinishedIntake(const TTank &CurrentTank, TStatusConstIterator StartFindIter)
{
    if (std::distance(StartFindIter, CurrentTank.TankStatus.end()) >= 16 ) {
        for (auto FirstIter = StartFindIter; FirstIter != std::prev(CurrentTank.TankStatus.end(), 16);  ++FirstIter) {
            const auto SecondIter = std::next(FirstIter, 10);
            if (SecondIter.value().Volume - FirstIter->Volume <= 0.0) {
               // qDebug() << "-->Find finished intake: " << FirstIter.key().toString("yyyy-MM-dd hh:mm:ss.zzz");
                return SecondIter;
            }
        }
    }
    return CurrentTank.TankStatus.end();
}

/*float TLevelGauge::GetValueFromCalibration(const TTank &CurrentTank, const float Height)
{
    //запрашиваемое значение есть в калибровочной таблице
    if (CurrentTank.TanksInfo.Calibration.find(Height) != CurrentTank.TanksInfo.Calibration.end())
        return CurrentTank.TanksInfo.Calibration[Height];

    auto Iter = CurrentTank.TanksInfo.Calibration.upperBound(Height);
    if (Iter == CurrentTank.TanksInfo.Calibration.end()) return CurrentTank.TanksInfo.Calibration.last();
    if (Iter == CurrentTank.TanksInfo.Calibration.begin()) return 0.0;

    //если не крайнии значение - делаем линейную интерполяцию
    auto PrevIter = std::prev(Iter);
    float Delta = ((Iter.value() - PrevIter.value()) / (Iter.key() - PrevIter.key())) * (Height - PrevIter.key());
    //qDebug() << "Prev: " << PrevIter.value() << "Delta:" << Delta << "Return:" << PrevIter.value() + Delta;
    return PrevIter.value() + Delta;
}*/

TLevelGauge::TStatusConstIterator TLevelGauge::GetRecordForSave(const QString &AZSCode, quint8 TankNumber)
{
    TTank &CurrentTank = AZSS[AZSCode].Tanks[TankNumber]; //получаем ссылку на активный резервуар

    QDateTime CurrentDateTime = QDateTime::currentDateTime(); //текуще время

    //добавляем записи в начале
    if (CurrentTank.TankStatus.firstKey() > std::min(CurrentTank.TanksInfo.LastSaveDateTime, CurrentTank.TanksInfo.LastIntakeDateTime))  {
        qDebug() << "-->No records found at the beginning";
        AddStatus(CurrentTank, std::min(CurrentTank.TanksInfo.LastSaveDateTime, CurrentTank.TanksInfo.LastIntakeDateTime).addSecs(-70), CurrentTank.TankStatus.firstKey(),
                  CurrentTank.TankStatus.first(), CurrentTank.TankStatus.first());
    }

    //ищем итератор на первую запись после записанной
    auto LastIterator = CurrentTank.TankStatus.upperBound(CurrentTank.TanksInfo.LastSaveDateTime); //получаеи итератор на последние не записанные данные

    //время с последней записи больше MaxpauseSec и новых записей нет
    //то добавляем записи равные последней сохраненной

    if (LastIterator == CurrentTank.TankStatus.end()) {
        if (CurrentTank.TanksInfo.LastSaveDateTime.secsTo(CurrentDateTime) > MaxPauseSec) {
            //SendLogMsg(MSG_CODE::CODE_INFORMATION, "No new entries found longer than MaxPause. AZS: " + AZSCode + " Tank: " + QString::number(TankNumber));
            qDebug() << "-->No new entries found longer than MaxPause. Last found record:" << CurrentTank.TankStatus.lastKey().toString("yyyy-MM-dd hh:mm:ss.zzz");
            //если последнего сохранненного статуса не найдена - то то берем последний измеренный
            if (CurrentTank.TanksInfo.LastWriteStatus.Height == -1) {
                CurrentTank.TanksInfo.LastWriteStatus = CurrentTank.TankStatus.last();
            }
            AddStatus(CurrentTank, CurrentTank.TanksInfo.LastSaveDateTime.addSecs(60), CurrentDateTime, CurrentTank.TanksInfo.LastWriteStatus, CurrentTank.TanksInfo.LastWriteStatus);
            LastIterator = CurrentTank.TankStatus.upperBound(CurrentTank.TanksInfo.LastSaveDateTime);
        }
        //если время паузы еще не прошло - просто выходим
        else {
            return CurrentTank.TankStatus.end();
        }
    }

    //если мы дошли до сюда  - LastIterator != CurrentTank.TankStatus.end()

    //Проверяем что у нас есть все записи
    //если не хватает записей в конце - они добавятся на следующей итерации

    //если не хватает записей в начале - добавляем их
    if (CurrentTank.TanksInfo.LastSaveDateTime.secsTo(LastIterator.key()) > 120) {
        qDebug() << "-->No entries found between: " << CurrentTank.TanksInfo.LastSaveDateTime.toString("yyyy-MM-dd hh:mm:ss.zzz") << " and "  <<
                                                                LastIterator.key().toString("yyyy-MM-dd hh:mm:ss.zzz");
        AddStatus(CurrentTank, CurrentTank.TanksInfo.LastSaveDateTime.addSecs(60), LastIterator.key(), CurrentTank.TanksInfo.LastWriteStatus, LastIterator.value());

        LastIterator = CurrentTank.TankStatus.upperBound(CurrentTank.TanksInfo.LastSaveDateTime); //получаеи итератор (сместился вперед)

    }

    //проверям наличие всех промежуточных записей
    for (auto Iter = LastIterator; Iter != std::prev(CurrentTank.TankStatus.end()); ++Iter) {
        auto NextIter = std::next(Iter); //итератор на следующую запись
        //если разница во времение между записями больше 100с - то добавляем недостоющие записи
        if (Iter.key().secsTo(NextIter.key()) > 120) {
            qDebug() << "-->No entries found between: " + Iter.key().toString("yyyy-MM-dd hh:mm:ss.zzz") + " and "  +
                        NextIter.key().toString("yyyy-MM-dd hh:mm:ss.zzz");

            //если уровень убывал или рос не сильно быстро (температурное расширение) то просто вставляем значения
            if (NextIter->Volume - Iter->Volume < DeltaIntakeVolume) {
                AddStatus(CurrentTank, Iter.key().addSecs(60), NextIter.key(), Iter.value(), NextIter.value());
            }
            //уровень наоборот на много - скорее всего мы пропустили слив
            else {
                qDebug() << "-->Add intake";
                if (Iter.key().secsTo(NextIter.key()) > 720) {  //проверяем - есть ли у нас место чтобы его вставить
                    AddStatus(CurrentTank, Iter.key().addSecs(60), Iter.key().addSecs(645), Iter.value(), NextIter.value());
                    AddStatus(CurrentTank, Iter.key().addSecs(660), NextIter.key(), NextIter.value(), NextIter.value());
                }
                else AddStatus(CurrentTank, Iter.key().addSecs(60), NextIter.key(), Iter.value(), NextIter.value());
            }
        }
    }

    //добавляем еще одну запись в начало если до LastIterator небыло данных
    if (LastIterator == CurrentTank.TankStatus.begin()) {
        qDebug() << "-->Add first record";
        AddStatus(CurrentTank, LastIterator.key().addSecs(-70), LastIterator.key(), LastIterator.value(), LastIterator.value());
    }

    //проверяем данные и сглаживаем если нужно 
    TestStatus(CurrentTank, std::prev(LastIterator));

    return LastIterator;
}

/*void TLevelGauge::AddNewCalibrationPoints(QList<TNewCalibration> NewCalibration)
{
    //Сохраняем в БД новые точки калибровки
    if (!NewCalibration.isEmpty()) {
        QSqlQuery QueryCalibtration(DB);
        DB.transaction();

        for (const auto& NewCalibrationItem : NewCalibration) {
            QString QueryCalibtrationText = "INSERT INTO [TanksCalibration] ([AZSCode], [TankNumber], [Height], [Volume], [Temp]) VALUES "
                                            "('" + NewCalibrationItem.AZSCode +  "', " +
                                            QString::number(NewCalibrationItem.TankNumber) +  ", " +
                                            QString::number(NewCalibrationItem.Height, 'f', 0)  + ", " +
                                            QString::number(NewCalibrationItem.Volume, 'f', 0)  + ", " +
                                            QString::number(NewCalibrationItem.Temp, 'f', 1)  + ")";


            if (!QueryCalibtration.exec(QueryCalibtrationText)) {
                qDebug() << "FAIL Cannot execute query. Error: " << QueryCalibtration.lastError().text() << " Query: "<< QueryCalibtration.lastQuery();
                DB.rollback();
                exit(-1);
            }
        }

        if (!DB.commit()) {
            qDebug() << "FAIL Cannot commit transation. Error: " << DB.lastError().text();
            DB.rollback();
            exit(-2);
        };

        qDebug() << "Save" << NewCalibration.size() << "new calibration points. Count points:" << NewCalibration.size();
    }
}
*/
void TLevelGauge::Start()
{
    //Подключение к БД с данными
    Config.beginGroup("DATABASE");
    DB = QSqlDatabase::addDatabase(Config.value("Driver", "QODBC").toString(), "MainDB");
    DB.setDatabaseName(Config.value("DataBase", "SystemMonitorDB").toString());
    DB.setUserName(Config.value("UID", "SYSDBA").toString());
    DB.setPassword(Config.value("PWD", "MASTERKEY").toString());
    DB.setConnectOptions(Config.value("ConnectionOprions", "").toString());
    DB.setPort(Config.value("Port", "3051").toUInt());
    DB.setHostName(Config.value("Host", "localhost").toString());
    Config.endGroup();

    if (!DB.open()) {
        qCritical() << "Cannot connect to database. Error: " << DB.lastError().text();
        exit(-10);
    };

    //подключение к БД для сохранения результатов
    Config.beginGroup("LEVELGAUGEDATABASE");
    LGDB = QSqlDatabase::addDatabase(Config.value("Driver", "QODBC").toString(), "LGDB");
    LGDB.setDatabaseName(Config.value("DataBase", "SystemMonitorDB").toString());
    LGDB.setUserName(Config.value("UID", "SYSDBA").toString());
    LGDB.setPassword(Config.value("PWD", "MASTERKEY").toString());
    LGDB.setConnectOptions(Config.value("ConnectionOprions", "").toString());
    LGDB.setPort(Config.value("Port", "3051").toUInt());
    LGDB.setHostName(Config.value("Host", "localhost").toString());
    Config.endGroup();

    if (!LGDB.open()) {
        qCritical() << "Cannot connect to database. Error: " << LGDB.lastError().text();
        exit(-11);
    };

    //устанавливаем режим работы таймера
    QObject::connect(&UpdateTimer, SIGNAL(timeout()), this, SLOT(onUpdate()));
    UpdateTimer.setSingleShot(false);

    Config.beginGroup("SYSTEM");
    UpdateTimer.setInterval(Config.value("LoadMeasuments", "60000").toUInt());
    //Считываем системные параметры
    MaxPauseSec = Config.value("MaxPauseSec", "600").toULongLong();
    //максимально допустимые перепады характеристик при работе
    MaxDelta.Density = Config.value("MaxDeltaDensity", "0.2").toFloat();
    MaxDelta.Temp = Config.value("MaxDeltaTemp", "0.1").toFloat();
    MaxDelta.Height = Config.value("MaxDeltaHeight", "10").toFloat();
    MaxDelta.Mass = Config.value("MaxDeltaMass", "40").toFloat();
    MaxDelta.Volume = Config.value("MaxDeltaVolume", "40").toFloat();
    //максимально допустимые перепады характеристик при поступление
    MaxIntakeDelta.Density = Config.value("MaxDeltaIntakeDensity", "0.5").toFloat();
    MaxIntakeDelta.Temp = Config.value("MaxDeltaIntakeTemp", "1.0").toFloat();
    MaxIntakeDelta.Height = Config.value("MaxDeltaIntakeHeight", "70").toFloat();
    MaxIntakeDelta.Mass = Config.value("MaxDeltaIntakeMass", "600").toFloat();
    MaxIntakeDelta.Volume = Config.value("MaxDeltaIntakeVolume", "700").toFloat();
    DeltaIntakeVolume = Config.value("DeltaIntakeVolume", "300").toFloat();
    Config.endGroup();

    //Создаем таймер записи логов
    QObject::connect(&WriteLogTimer, SIGNAL(timeout()), this, SLOT(onWriteLogTimer()));
    WriteLogTimer.start(30000);

    SendLogMsg(TDBLoger::MSG_CODE::CODE_OK, "Successfully started");

    LoadTanksInfo();
    LoadLastWriteData();
    onUpdate(); //выполняем первый запуск
    UpdateTimer.start(); //запускаем таймер
}

void TLevelGauge::Stop()
{
    //тормозим таймер сохранения данных
    UpdateTimer.stop();

    LGDB.close();

    //записываем в лог сообщение об остановке
    SendLogMsg(MSG_CODE::CODE_OK, "Successfully finished");
    WriteLogTimer.stop();
    onWriteLogTimer();

    DB.close();
}

void TLevelGauge::onUpdate()
{ 
    LoadMeasuments(); //загрузаем последние поступившие данные
    SaveMeasuments(); //сохраняем данных
    SaveIntake();     //сохраняем информацию о приходах

    #ifdef QT_DEBUG
    QFile file("Values.log");
    if (file.open(QIODevice::WriteOnly)) {
        QTextStream tmp(&file);
        for (const auto &AZSCode : AZSS.keys()) {
            tmp << "=================================================================================================================================================\n";
            for (const auto &TankNumber : AZSS[AZSCode].Tanks.keys()) {
                tmp << "======AZS:" << AZSCode << " TANK:" << QString::number(TankNumber) << "\n";
                TTank &CurrentTank = AZSS[AZSCode].Tanks[TankNumber];
                for (const auto &DateTimeItem : CurrentTank.TankStatus.keys()) {
                    tmp << DateTimeItem.toString("yyyy-MM-dd hh:mm:ss.zzz") <<
                           "->Volume=" <<  CurrentTank.TankStatus[DateTimeItem].Volume << " Height=" << CurrentTank.TankStatus[DateTimeItem].Height << " Density:" <<  CurrentTank.TankStatus[DateTimeItem].Density
                        << " Mass=" <<  CurrentTank.TankStatus[DateTimeItem].Mass << " Temp=" <<  CurrentTank.TankStatus[DateTimeItem].Temp << " Flag:" << CurrentTank.TankStatus[DateTimeItem].Flag << "\n";
                }
            }
        }
        file.close();
    }
    #endif
}

void TLevelGauge::onWriteLogTimer()
{
    //   QTime time = QTime::currentTime();
   if (!LogQueue.isEmpty()) {

       QSqlQuery QueryLog(DB);
       DB.transaction();

       QString QueryText = "INSERT INTO LOG ([CATEGORY], [UID], [DateTime], [SENDER], [MSG]) VALUES ";

       while (!LogQueue.isEmpty()) {
           TLogMsg tmp = LogQueue.dequeue();
           QueryText += "(" +
                        QString::number(tmp.Category) + ", "
                        "'" + tmp.UID + "', "
                        "CAST('" + tmp.DateTime.toString("yyyy-MM-dd hh:mm:ss.zzz") + "' AS DATETIME2), "
                        "'" + tmp.Sender +"', "
                        "'" + tmp.Msg +"'), ";
       }
       QueryText = QueryText.left(QueryText.length() - 2); //удаляем лишнюю запятую в конце

       if (!QueryLog.exec(QueryText)) {
           qCritical() << "FAIL Cannot execute query. Error: " << QueryLog.lastError().text() << " Query: "<< QueryLog.lastQuery();
           DB.rollback();
           exit(-1);
       }
       if (!DB.commit()) {
           qCritical() << "FAIL Cannot commit transation. Error: " << DB.lastError().text();
           DB.rollback();
           exit(-2);
       };

       #ifdef QT_DEBUG
       qDebug() << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << "Log messages save successull";
       #endif
   }
   #ifdef QT_DEBUG
   else {
       qDebug() << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << "No log messages";
   }
   #endif
    //   qDebug() << "WriteLogTime" << time.msecsTo(QTime::currentTime()) << "ms";
}


