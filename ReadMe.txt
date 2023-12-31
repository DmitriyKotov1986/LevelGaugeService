В данном проекте реализован Windows-сервис для передачи показаний телеметрии (уровень, объем, масса, плотность, температура нефтепродукта) с уровнемеров в сторонюю БД.

Общее описание:
Данная служба является частью программного комплекса по сбору, систематизации и контролю телеметрии с уровнемеров, расположенных на АЗС заказчика.
Одна из задач выполняемая комплесом - передача показаний уровнемеров в режиме on-line в государственные контролирующие органы Республики Казахстан.

Комплекс состоит из слудующих частей:
    1. Службы получения данных телеметрии с уровнемеров расположенная непосредственно на АЗС. Она с периодичностью раз минуту оправшивает уровнемер и передает данные на
HTTP сервер по каналам связи (обычно это GSM).
    2. HTTP сервер сохраняет полученные данные в БД HTTP сервера
    3. Модуль для показаний из БД HTTP сервера в БД 1С
    3. Служба для контроля работы уровнемеров сервисной службой заказчика (реализовано как telegram bot)
    4. Служба передачи показаний из БД HTTP сервера в БД контролирующих органов (реализованна в текущем проекте)

Как это работает:
После запуска служба считывает из БД конфигурацию АЗС и резервуаров на их и последних сохраненных данных.
Далее раз в минуту производится чтение текущих показаний уровнемеров из БД HTTP сервера. Если данные корректны (нет резких перепадов/значения не выходят за допустимые границы,
данные актуальны на текущий момент и т.д.- они сохраняются в результирующую БД. После этого по характерному изменению уровня, происходит поиск приемов топлива на АЗС. Если слив
топлива обнаружен и завершился - то производится расчет количества принятого топлива и сохранение его параметров в БД.
Поскольку требуется передача данных в контроллирующте органы в режиме on-line, а обеспечить надежную связь с АЗС не предоставляется возможным, то в программе реализован алгоритм
подстановки недостающих данных. Реализован он следующим образом: 1. при запуске службы: если обнаруживается пробел в данных и уровень топлива (и прочих данных) в конце недостающего
периода меньше, чем в начале (можно предположить что приемов топлива в это время небыло) - то добавляется плавный спад уровня уровня с линейной интерполяцией. Если за этот период
уровень вырос - то добавляется характерное для прием топлива изменение уровня с последующим плавным убыванием; 2. в процессе работы: если нет новых данных от АЗС на протяжении
более чем 10 минут - то данные интерполируются на эти 10 минут с теми же значениями что и были измерены в последний раз. При этом в добавлямые данные вносится случайная погрешность чтобы
соответствовать реальному поведению физических параметров.
Так же в конфигурации резервуаров можно указать периоды когда данные не нужно передавать (например при проведении офффициального сервисного обслуживания)

Структура проекта:
В папке QtService содержиться немного доработанный и исправленный OpenSource проект https://skycoder42.github.io/QtService/. В нем реализован основной механизм работы службы и ее
взаимодействия с ОС.

LevelGaugeService.pro - файл проекта QT
main.cpp - основной файл программы
service.cpp - класс службы
tlevelgauge.cpp - основной класс осуществляющий обработку и передачу показаний
ReadMe.txt - файл с описанием
