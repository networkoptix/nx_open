<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.0" language="ru" sourcelanguage="en">
<context>
    <name>Language</name>
    <message>
        <location filename="../src/translation/translation_manager.cpp" line="156"/>
        <source>Locale Code</source>
        <extracomment>Internal. Please don&apos;t change existing translation.</extracomment>
        <translation>ru</translation>
    </message>
    <message>
        <location filename="../src/translation/translation_manager.cpp" line="153"/>
        <source>Language Name</source>
        <extracomment>Language name that will be displayed to user. Must not be empty.</extracomment>
        <translation>Русский</translation>
    </message>
</context>
<context>
    <name>Qee::Evaluator</name>
    <message>
        <location filename="../src/utils/common/evaluator.cpp" line="722"/>
        <source>Invalid stack size after program evaluation: %1.</source>
        <translation>Некорректный размер стека после оценки программы: %1.</translation>
    </message>
    <message>
        <location filename="../src/utils/common/evaluator.cpp" line="764"/>
        <source>Could not deduce result type for operation %1(&apos;%2&apos;, &apos;%3&apos;).</source>
        <translation>Невозможно сделать вывод о типе результата операции %1(&apos;%2&apos;, &apos;%3&apos;).</translation>
    </message>
    <message>
        <location filename="../src/utils/common/evaluator.cpp" line="792"/>
        <source>Invalid parameter type for operation %1(&apos;%2&apos;, &apos;%2&apos;)</source>
        <translation>Некорректный тип параметра операции %1(&apos;%2&apos;, &apos;%2&apos;)</translation>
    </message>
    <message>
        <location filename="../src/utils/common/evaluator.cpp" line="802"/>
        <source>Could not deduce arithmetic supertype for type &apos;%1&apos;.</source>
        <translation>Невозможно определить арифметический супертип для типа &apos;%1&apos;.</translation>
    </message>
    <message>
        <location filename="../src/utils/common/evaluator.cpp" line="824"/>
        <source>Invalid parameter type for operation %1(&apos;%2&apos;)</source>
        <translation>Некорректный тип параметра операции %1(&apos;%2&apos;)</translation>
    </message>
    <message>
        <location filename="../src/utils/common/evaluator.cpp" line="832"/>
        <source>Argument number for %1 instruction has invalid type &apos;%2&apos;.</source>
        <translation>Число аргументов для %1 инструкций имеет недопустимый тип &apos;%2&apos;.</translation>
    </message>
    <message>
        <location filename="../src/utils/common/evaluator.cpp" line="836"/>
        <source>Argument number for %1 instruction is invalid.</source>
        <translation>Некорректное число аргументов для %1 инструкций.</translation>
    </message>
    <message>
        <location filename="../src/utils/common/evaluator.cpp" line="841"/>
        <source>Stack underflow during execution of %1 instruction.</source>
        <translation>Обращение к несуществующей области стека при выполнении %1 инструкции.</translation>
    </message>
    <message>
        <location filename="../src/utils/common/evaluator.cpp" line="845"/>
        <source>Function name for %1 instruction has invalid type &apos;%2&apos;.</source>
        <translation>Имя функции для %1 инструкции имеет недопустимый тип &apos;%2&apos;.</translation>
    </message>
    <message>
        <location filename="../src/utils/common/evaluator.cpp" line="856"/>
        <source>Function or variable &apos;%1&apos; is not defined.</source>
        <translation>Функция или переменная &apos;%1&apos; не определена.</translation>
    </message>
    <message>
        <location filename="../src/utils/common/evaluator.cpp" line="862"/>
        <source>Variable &apos;%1&apos; is not a function and cannot be called.</source>
        <translation>Переменная &apos;%1&apos; не является функцией и не может быть вызвана.</translation>
    </message>
    <message>
        <location filename="../src/utils/common/evaluator.cpp" line="874"/>
        <source>Stack underflow during program evaluation.</source>
        <translation>Обращение к несуществующей области стека при оценки программы.</translation>
    </message>
</context>
<context>
    <name>Qee::Lexer</name>
    <message>
        <location filename="../src/utils/common/evaluator.cpp" line="113"/>
        <source>Unexpected symbol &apos;%1&apos; at position %2.</source>
        <translation>Недопустимый символ &apos;%1&apos; в позиции %2.</translation>
    </message>
</context>
<context>
    <name>Qee::ParameterPack</name>
    <message>
        <location filename="../src/utils/common/evaluator.h" line="185"/>
        <location filename="../src/utils/common/evaluator.h" line="192"/>
        <source>Parameter %2 is not specified for function &apos;%1&apos;.</source>
        <translation>Параметр %2 не определён для функции &apos;%1&apos;.</translation>
    </message>
    <message>
        <location filename="../src/utils/common/evaluator.h" line="195"/>
        <source>Parameter %2 of function &apos;%1&apos; is of type &apos;%3&apos;, but type &apos;%4&apos; was expected.</source>
        <translation>Параметр %2 функции &apos;%1&apos; имеет тип &apos;%3&apos; вместо типа &apos;%4&apos;.</translation>
    </message>
    <message>
        <location filename="../src/utils/common/evaluator.h" line="201"/>
        <source>Function &apos;%1&apos; is expected to have %3 arguments, %2 provided.</source>
        <translation>Функция &apos;%1&apos; должна получать %3 аргумента, а задано %2.</translation>
    </message>
    <message>
        <location filename="../src/utils/common/evaluator.h" line="206"/>
        <source>Function &apos;%1&apos; is expected to have %3-%4 arguments, %2 provided.</source>
        <translation>Функция &apos;%1&apos; должна получать %3-%4 аргумента, а задано %2.</translation>
    </message>
</context>
<context>
    <name>Qee::Parser</name>
    <message>
        <location filename="../src/utils/common/evaluator.cpp" line="179"/>
        <source>Unexpected token %1 (&apos;%2&apos;) at position %3.</source>
        <translation>Недопустимая метка %1 (&apos;%2&apos;) в позиции %3.</translation>
    </message>
    <message>
        <location filename="../src/utils/common/evaluator.cpp" line="236"/>
        <source>Invalid color constant &apos;%1&apos;.</source>
        <translation>Недопустимая цветовая константа &apos;%1&apos;.</translation>
    </message>
    <message>
        <location filename="../src/utils/common/evaluator.cpp" line="281"/>
        <source>Invalid number constant &apos;%1&apos;.</source>
        <translation>Недопустимая числовая константа &apos;%1&apos;.</translation>
    </message>
</context>
<context>
    <name>QnAbstractStorageResource</name>
    <message>
        <location filename="../src/core/resource/abstract_storage_resource.cpp" line="120"/>
        <source>Windows Network Shared Resource</source>
        <translation>Общий ресурс сети Windows</translation>
    </message>
    <message>
        <location filename="../src/core/resource/abstract_storage_resource.cpp" line="121"/>
        <source>\\&lt;Computer Name&gt;\&lt;Folder&gt;</source>
        <translation>\\&lt;Имя компьютера&gt;\&lt;Папка&gt;</translation>
    </message>
    <message>
        <location filename="../src/core/resource/abstract_storage_resource.cpp" line="125"/>
        <source>Coldstore Network Storage</source>
        <translation>Сетевое хранилище Coldstore</translation>
    </message>
    <message>
        <location filename="../src/core/resource/abstract_storage_resource.cpp" line="126"/>
        <source>coldstore://&lt;Address&gt;</source>
        <translation>coldstore://&lt;адрес&gt;</translation>
    </message>
</context>
<context>
    <name>QnBusinessStringsHelper</name>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="64"/>
        <source>User Defined (%1)</source>
        <translation>Определено пользователем (%1)</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="68"/>
        <source>Motion on Camera</source>
        <translation>Детектор движения на камере</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="69"/>
        <source>Input Signal on Camera</source>
        <translation>Тревожный вход на камере</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="70"/>
        <source>Camera Disconnected</source>
        <translation>Камера отключена</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="71"/>
        <source>Storage Failure</source>
        <translation>Ошибка хранилища</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="72"/>
        <source>Network Issue</source>
        <translation>Проблема с сетью</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="73"/>
        <source>Camera IP Conflict</source>
        <translation>Конфликт IP адреса камеры</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="95"/>
        <source>Camera %1 was disconnected</source>
        <translation>Камера %1 отключилась</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="98"/>
        <source>Input on %1</source>
        <translation>Тревожный вход на %1</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="101"/>
        <source>Motion on %1</source>
        <translation>Движение на %1</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="104"/>
        <source>Storage Failure at %1</source>
        <translation>Ошибка хранилища %1</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="107"/>
        <source>Network Issue at %1</source>
        <translation>Проблема с сетью на %1</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="113"/>
        <source>Camera IP Conflict at %1</source>
        <translation>Конфликт IP адреса на сервере %1</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="45"/>
        <source>Camera output</source>
        <translation>Тревожный выход</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="46"/>
        <source>Camera output for 30 sec</source>
        <translation>Тревожный выход в течение 30 секунд</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="47"/>
        <source>Bookmark</source>
        <translation>Закладка</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="48"/>
        <source>Camera recording</source>
        <translation>Запись с камеры</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="49"/>
        <source>Panic recording</source>
        <translation>Запись по тревоге</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="51"/>
        <source>Write to log</source>
        <translation>Записать в журнал</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="52"/>
        <source>Show notification</source>
        <translation>Показать оповещение</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="54"/>
        <source>Play sound</source>
        <translation>Воспроизвести звук</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="53"/>
        <source>Repeat sound</source>
        <translation>Повторить звук</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="50"/>
        <source>Send email</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="55"/>
        <source>Speak</source>
        <translation>Произнести</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="56"/>
        <source>Unknown (%1)</source>
        <translation>Неизвестно (%1)</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="78"/>
        <source>Any Camera Issue</source>
        <translation>Любая проблема с камерой</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="79"/>
        <source>Any Server Issue</source>
        <translation>Любая проблема с сервером</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="80"/>
        <source>Any Event</source>
        <translation>Любое событие</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="92"/>
        <source>Undefined event has occurred on %1</source>
        <translation>Неопределённое событие возникло на %1</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="74"/>
        <source>Server Failure</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="75"/>
        <source>Server Conflict</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="76"/>
        <source>Server Started</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="77"/>
        <source>License Issue</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="110"/>
        <source>Server &quot;%1&quot; Failure</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="116"/>
        <source>Server &quot;%1&quot; Conflict</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="119"/>
        <source>Server &quot;%1&quot; Started</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="121"/>
        <source>Server &quot;%1&quot; had license issue</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="126"/>
        <source>Unknown event has occurred</source>
        <translation>Произошло неизвестное событие</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="144"/>
        <source>Event: %1</source>
        <translation>Событие: %1</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="147"/>
        <source>Source: %1</source>
        <translation>Источник: %1</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="151"/>
        <source>Url: %1</source>
        <translation>Адрес: %1</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="190"/>
        <source>Input port: %1</source>
        <translation>Тревожный вход: %1</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="198"/>
        <source>Reason: %1</source>
        <translation>Причина: %1</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="202"/>
        <source>Conflict address: %1</source>
        <translation>Конфликтующий адрес: %1</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="207"/>
        <location filename="../src/business/business_strings_helper.cpp" line="223"/>
        <source>Camera #%1 MAC: %2</source>
        <translation>MAC адрес камеры %1: %2</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="219"/>
        <source>Conflicting Server #%1: %2</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="370"/>
        <source>Connection to camera (primary stream) was unexpectedly closed.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="372"/>
        <source>Connection to camera (secondary stream) was unexpectedly closed.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="408"/>
        <source>Recording on %n camera(s) is disabled: </source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="325"/>
        <source>%2 %1</source>
        <comment>%1 means time, %2 means date</comment>
        <translation>%2 %1</translation>
    </message>
    <message numerus="yes">
        <location filename="../src/business/business_strings_helper.cpp" line="329"/>
        <source>%n times, first: %2 %1</source>
        <comment>%1 means time, %2 means date</comment>
        <translation>
            <numerusform>%n раз, впервые %2 %1</numerusform>
            <numerusform>%n раза, впервые %2 %1</numerusform>
            <numerusform>%n раз, впервые %2 %1</numerusform>
        </translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="340"/>
        <source>Time: %1 on %2</source>
        <comment>%1 means time, %2 means date</comment>
        <translation>Время: %2 %1</translation>
    </message>
    <message numerus="yes">
        <location filename="../src/business/business_strings_helper.cpp" line="344"/>
        <source>First occurrence: %1 on %2 (%n times total)</source>
        <comment>%1 means time, %2 means date</comment>
        <translation>
            <numerusform>Первое проявление: %2 %1 (всего %n раз)</numerusform>
            <numerusform>Первое проявление: %2 %1 (всего %n раза)</numerusform>
            <numerusform>Первое проявление: %2 %1 (всего %n раз)</numerusform>
        </translation>
    </message>
    <message numerus="yes">
        <location filename="../src/business/business_strings_helper.cpp" line="364"/>
        <source>No video frame received during last %n seconds.</source>
        <translation>
            <numerusform>В течение %n с не было получено ни одного кадра.</numerusform>
            <numerusform>В течение %n с не было получено ни одного кадра.</numerusform>
            <numerusform>В течение %n с не было получено ни одного кадра.</numerusform>
        </translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="378"/>
        <source>RTP packet loss detected, prev seq.=%1 next seq.=%2.</source>
        <translation>Обнаружена потеря RTP пакетов, предыдущий пакет%1, следующий пакет %2.</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="380"/>
        <source>RTP packet loss detected.</source>
        <translation>Обнаружена потеря RTP пакетов.</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="384"/>
        <source>Server terminated.</source>
        <translation>Сервер завершил работу.</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="388"/>
        <source>Server started after crash.</source>
        <translation>Сервер запустился после сбоя.</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="393"/>
        <source>I/O error has occurred at %1.</source>
        <translation>Ошибка ввода/вывода на %1.</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="398"/>
        <source>Not enough HDD/SSD speed for recording to %1.</source>
        <translation>Не хватает скорости HDD/SSD для записи на %1.</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="403"/>
        <source>HDD/SSD disk %1 is full. Disk contains too much data that is not managed by VMS.</source>
        <translation>Диск %1 переполнен. Диск содержит слишком много данных, не относящихся к системе.</translation>
    </message>
</context>
<context>
    <name>QnCameraDiagnosticsErrorCodeStrings</name>
    <message>
        <location filename="../src/utils/camera/camera_diagnostics.cpp" line="130"/>
        <source>(unknown)</source>
        <translation>(неизвестно)</translation>
    </message>
    <message>
        <location filename="../src/utils/camera/camera_diagnostics.cpp" line="27"/>
        <source>OK</source>
        <translation>ОК</translation>
    </message>
    <message>
        <location filename="../src/utils/camera/camera_diagnostics.cpp" line="31"/>
        <source>Server %1 is not available.
 Check that Server is up and running.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/utils/camera/camera_diagnostics.cpp" line="35"/>
        <source>Received bad response from Server %1: &quot;%2&quot;.
 Check if Server is up and has the proper version.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/utils/camera/camera_diagnostics.cpp" line="39"/>
        <source>Cannot connect to http port %1.
 Make sure the camera is plugged into the network.</source>
        <translation>Невозможно подключиться к HTTP порту %1. Убедитесь, что камера подключена к сети.</translation>
    </message>
    <message>
        <location filename="../src/utils/camera/camera_diagnostics.cpp" line="43"/>
        <source>Cannot open media url %1. Failed to connect to media port %2.
 Make sure port %2 is accessible (e.g. forwarded). Please try to reboot the camera, then restore factory defaults on the web-page.</source>
        <translation>Невозвожно открыть адрес %1. Невозможно подключиться к медиа порту %2.
 Убедитесь, что порт %2 доступен (проброшен, и т.п.). Попробуйте перезагрузить камеру, а затем сбросить к заводским настройкам через web-интерфейс.</translation>
    </message>
    <message>
        <location filename="../src/utils/camera/camera_diagnostics.cpp" line="48"/>
        <source>Cannot open media url %1. Connection to port %2 was closed unexpectedly.
 Make sure the camera is plugged into the network. Try to reboot the camera.</source>
        <translation>Невозвожно открыть адрес %1. Подключение к порту %2 было сброшено.
 Убедитесь, что камера подключена к сети. Попробуйте перезагрузить камеру.</translation>
    </message>
    <message>
        <location filename="../src/utils/camera/camera_diagnostics.cpp" line="53"/>
        <source>Could not parse camera response. Url %1, request name %2.
 Please try to reboot the camera, then restore factory defaults on the web-page. Finally, try to update firmware. If the problem persists, please contact support.</source>
        <translation>Не распознан ответ от камерв. Адрес %1, запрос %2.
 Попробуйте перезагрузить камеру, затем восстановить заводские настройки или обновить прошивку. Если проблема сохранится, свяжитесь с техподдержкой.</translation>
    </message>
    <message>
        <location filename="../src/utils/camera/camera_diagnostics.cpp" line="58"/>
        <source>No supported media tracks at url %1.
 Please try to reboot the camera, then restore factory defaults on the web-page. Finally, try to update firmware. If the problem persists, please contact support.</source>
        <translation>Неподдерживаемый поток по адресу %1.
 Попробуйте перезагрузить камеру, затем восстановить заводские настройки или обновить прошивку. Если проблема сохранится, свяжитесь с техподдержкой.</translation>
    </message>
    <message>
        <location filename="../src/utils/camera/camera_diagnostics.cpp" line="63"/>
        <source>Not authorized. Url %1.</source>
        <translation>Ошибка авторизации. Адрес %1.</translation>
    </message>
    <message>
        <location filename="../src/utils/camera/camera_diagnostics.cpp" line="67"/>
        <source>Cannot open media url %1. Unsupported media protocol %2.
 Please try to restore factory defaults on the web-page. Finally, try to update firmware. If the problem persists, please contact support.</source>
        <translation>Невозможно открыть адрес %1. Протокол %2 не поддерживается.
 Попробуйте перезагрузить камеру, затем восстановить заводские настройки или обновить прошивку. Если проблема сохранится, свяжитесь с техподдержкой.</translation>
    </message>
    <message>
        <location filename="../src/utils/camera/camera_diagnostics.cpp" line="72"/>
        <source>Failed to configure parameter %1.
 First, try to turn on recording (if it&apos;s off) and decrease fps in camera settings. If it doesn&apos;t help, restore factory defaults on the camera web-page. If the problem persists, please contact support.</source>
        <translation>Невозможно настроить параметр %1.
 Попробуйте включить запись (если она отключена) и уменьшить частоту кадров в настройках камеры. Если это не помогло, восстановите заводские настройки в веб-интерфейсе камеры. Если проблема сохранится, свяжитесь с техподдержкой.</translation>
    </message>
    <message>
        <location filename="../src/utils/camera/camera_diagnostics.cpp" line="77"/>
        <source>Camera request &quot;%1&quot; failed with error &quot;%2&quot;.
 Please try to reboot the camera, then restore factory defaults on the web-page. Finally, try to update firmware. If the problem persists, please contact support.</source>
        <translation>Запрос &quot;%1&quot; завершился ошибкой &quot;%2&quot;.
 Попробуйте перезагрузить камеру, затем восстановить заводские настройки или обновить прошивку. Если проблема сохранится, свяжитесь с техподдержкой.</translation>
    </message>
    <message>
        <location filename="../src/utils/camera/camera_diagnostics.cpp" line="82"/>
        <source>Unknown Camera Issue.
 Please contact support.</source>
        <translation>Неизвестная проблема с камерой.
 Пожалуйста, свяжитесь с техподдержкой.</translation>
    </message>
    <message>
        <location filename="../src/utils/camera/camera_diagnostics.cpp" line="86"/>
        <source>An input/output error has occurred. OS message: &quot;%1&quot;.
 Make sure the camera is plugged into the network. Try to reboot the camera.</source>
        <translation>Ошибка ввода/вывода. Сообщение ОС: &quot;%1&quot;.
 Убедитесь, что камера подключена к сети. Попробуйте перезагрузить камеру.</translation>
    </message>
    <message>
        <location filename="../src/utils/camera/camera_diagnostics.cpp" line="93"/>
        <source>Invalid data was received from the camera: %1.</source>
        <translation>Некорректные данные получены от камеры %1.</translation>
    </message>
    <message>
        <location filename="../src/utils/camera/camera_diagnostics.cpp" line="96"/>
        <source>Too many media errors. Please open camera issues dialog for more details.</source>
        <translation>Слишком много ошибок потока. Откройте диалог ошибок камеры, чтобы получить подробную информацию.</translation>
    </message>
    <message>
        <location filename="../src/utils/camera/camera_diagnostics.cpp" line="99"/>
        <source>Media stream is opened but no media data was received.</source>
        <translation>Поток подключен, но данные не поступают.</translation>
    </message>
    <message>
        <location filename="../src/utils/camera/camera_diagnostics.cpp" line="102"/>
        <source>Camera initialization process in progress</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/utils/camera/camera_diagnostics.cpp" line="117"/>
        <source>

Parameters: </source>
        <translation>Параметры: </translation>
    </message>
    <message>
        <location filename="../src/utils/camera/camera_diagnostics.cpp" line="89"/>
        <source>Server has been stopped.</source>
        <translation>Сервер был выключен.</translation>
    </message>
    <message>
        <location filename="../src/utils/camera/camera_diagnostics.cpp" line="115"/>
        <source>Unknown error. Please contact support.</source>
        <translation>Неизвестная ошибка. Пожалуйста, свяжитесь с техподдержкой.</translation>
    </message>
</context>
<context>
    <name>QnCommandLineParser</name>
    <message>
        <location filename="../src/utils/common/command_line_parser.cpp" line="154"/>
        <source>No value provided for the &apos;%1&apos; argument.</source>
        <translation>Не задано значение параметра %1.</translation>
    </message>
    <message>
        <location filename="../src/utils/common/command_line_parser.cpp" line="166"/>
        <source>Invalid value for &apos;%1&apos; argument - expected %2, provided &apos;%3&apos;.</source>
        <translation>Неверное значение параметра %1. Ожидается %2, введено %3.</translation>
    </message>
</context>
<context>
    <name>QnEnvironment</name>
    <message>
        <location filename="../src/utils/common/environment.cpp" line="58"/>
        <source>Launching Windows Explorer failed</source>
        <translation>Ошибка запуска Проводника Windows</translation>
    </message>
    <message>
        <location filename="../src/utils/common/environment.cpp" line="59"/>
        <source>Could not find explorer.exe in path to launch Windows Explorer.</source>
        <translation>Не найден путь к explorer.exe для запуска Проводника Windows.</translation>
    </message>
</context>
<context>
    <name>QnFfmpegAudioTranscoder</name>
    <message>
        <location filename="../src/transcoding/ffmpeg_audio_transcoder.cpp" line="53"/>
        <source>Audio context was not specified.</source>
        <translation>Аудиопоток не был определён.</translation>
    </message>
    <message>
        <location filename="../src/transcoding/ffmpeg_audio_transcoder.cpp" line="67"/>
        <source>Could not find encoder for codec %1.</source>
        <translation>Не найден кодировщик для кодек %1.</translation>
    </message>
    <message>
        <location filename="../src/transcoding/ffmpeg_audio_transcoder.cpp" line="94"/>
        <source>Could not initialize audio encoder.</source>
        <translation>Невозможно инициализировать аудио кодировщик.</translation>
    </message>
    <message>
        <location filename="../src/transcoding/ffmpeg_audio_transcoder.cpp" line="103"/>
        <source>Could not initialize audio decoder.</source>
        <translation>Невозможно инициализировать аудио декодер.</translation>
    </message>
</context>
<context>
    <name>QnFfmpegTranscoder</name>
    <message>
        <location filename="../src/transcoding/ffmpeg_transcoder.cpp" line="176"/>
        <source>Container %1 was not found in FFMPEG library.</source>
        <translation>Контейнер %1 не был найден в библиотеке FFMPEG.</translation>
    </message>
    <message>
        <location filename="../src/transcoding/ffmpeg_transcoder.cpp" line="185"/>
        <source>Could not create output context for format %1.</source>
        <translation>Невозможно создать выходной поток для формата %1.</translation>
    </message>
    <message>
        <location filename="../src/transcoding/ffmpeg_transcoder.cpp" line="204"/>
        <location filename="../src/transcoding/ffmpeg_transcoder.cpp" line="284"/>
        <source>Could not allocate output stream for recording.</source>
        <translation>Невозможно создать буфер для записи потока.</translation>
    </message>
    <message>
        <location filename="../src/transcoding/ffmpeg_transcoder.cpp" line="245"/>
        <source>Could not perform direct stream copy because frame size is undefined.</source>
        <translation>Ошибка перекодировщика: для прямого копирования потока должен быть задан размер кадра.</translation>
    </message>
    <message>
        <location filename="../src/transcoding/ffmpeg_transcoder.cpp" line="292"/>
        <source>Could not find codec %1.</source>
        <translation>Не найден кодек %1.</translation>
    </message>
    <message>
        <location filename="../src/transcoding/ffmpeg_transcoder.cpp" line="330"/>
        <source>Video or audio codec is incompatible with container %1.</source>
        <translation>Видео или аудио кодек несовместим с форматом %1.</translation>
    </message>
</context>
<context>
    <name>QnFfmpegVideoTranscoder</name>
    <message>
        <location filename="../src/transcoding/ffmpeg_video_transcoder.cpp" line="59"/>
        <source>Could not find encoder for codec %1.</source>
        <translation>Не найден кодировщик для кодекa %1.</translation>
    </message>
    <message>
        <location filename="../src/transcoding/ffmpeg_video_transcoder.cpp" line="106"/>
        <source>Could not initialize video encoder.</source>
        <translation>Невозможно инициализировать видео кодировщик.</translation>
    </message>
</context>
<context>
    <name>QnLicense</name>
    <message>
        <location filename="../src/licensing/license.cpp" line="151"/>
        <source>Trial</source>
        <translation>Временная</translation>
    </message>
    <message>
        <location filename="../src/licensing/license.cpp" line="152"/>
        <source>Analog</source>
        <translation>Аналоговая</translation>
    </message>
    <message>
        <location filename="../src/licensing/license.cpp" line="153"/>
        <source>Professional</source>
        <translation>Полная</translation>
    </message>
    <message>
        <location filename="../src/licensing/license.cpp" line="154"/>
        <source>Edge</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/licensing/license.cpp" line="155"/>
        <source>Vmax</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/licensing/license.cpp" line="156"/>
        <source>Analog encoder</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/licensing/license.cpp" line="157"/>
        <source>Video Wall</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/licensing/license.cpp" line="170"/>
        <source>Trial licenses</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/licensing/license.cpp" line="171"/>
        <source>Analog licenses</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/licensing/license.cpp" line="172"/>
        <source>Professional licenses</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/licensing/license.cpp" line="173"/>
        <source>Edge licenses</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/licensing/license.cpp" line="174"/>
        <source>Vmax licenses</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/licensing/license.cpp" line="175"/>
        <source>Analog encoder licenses</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/licensing/license.cpp" line="176"/>
        <source>Video Wall licenses</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/licensing/license.cpp" line="343"/>
        <source>Invalid signature</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/licensing/license.cpp" line="345"/>
        <source>Server with necessary hardware ID is not found</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/licensing/license.cpp" line="347"/>
        <source>Invalid customization</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/licensing/license.cpp" line="349"/>
        <source>Expired</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/licensing/license.cpp" line="351"/>
        <source>Invalid type</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/licensing/license.cpp" line="353"/>
        <source>Only single license is allowed for this device</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/licensing/license.cpp" line="355"/>
        <source>Unknown error</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>QnLicenseUsageHelper</name>
    <message numerus="yes">
        <location filename="../src/utils/license_usage_helper.cpp" line="75"/>
        <source>%n %2 are used out of %1.</source>
        <translation type="unfinished">
            <numerusform></numerusform>
            <numerusform></numerusform>
            <numerusform></numerusform>
        </translation>
    </message>
    <message numerus="yes">
        <location filename="../src/utils/license_usage_helper.cpp" line="94"/>
        <source>%n %2 will be used out of %1.</source>
        <translation type="unfinished">
            <numerusform></numerusform>
            <numerusform></numerusform>
            <numerusform></numerusform>
        </translation>
    </message>
    <message numerus="yes">
        <location filename="../src/utils/license_usage_helper.cpp" line="112"/>
        <source>Activate %n more %2. </source>
        <translation type="unfinished">
            <numerusform></numerusform>
            <numerusform></numerusform>
            <numerusform></numerusform>
        </translation>
    </message>
    <message numerus="yes">
        <location filename="../src/utils/license_usage_helper.cpp" line="114"/>
        <source>%n more %2 will be used. </source>
        <translation type="unfinished">
            <numerusform></numerusform>
            <numerusform></numerusform>
            <numerusform></numerusform>
        </translation>
    </message>
    <message>
        <location filename="../src/utils/license_usage_helper.cpp" line="186"/>
        <source>There was a problem activating your license key. Database error has occurred.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/utils/license_usage_helper.cpp" line="188"/>
        <source>There was a problem activating your license key. Invalid data received. Please contact support team to report issue.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/utils/license_usage_helper.cpp" line="190"/>
        <source>The license key you have entered is invalid. Please check that license key is entered correctly. If problem continues, please contact support team to confirm if license key is valid or to get a valid license key.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/utils/license_usage_helper.cpp" line="193"/>
        <source>You are trying to activate an incompatible license with your software. Please contact support team to get a valid license key.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/utils/license_usage_helper.cpp" line="195"/>
        <source>This license key has been previously activated to hardware id {{hwid}} on {{time}}. Please contact support team to get a valid license key.</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>QnMediaServerResource</name>
    <message>
        <location filename="../src/core/resource/media_server_resource.cpp" line="44"/>
        <source>Server</source>
        <translation>Сервер</translation>
    </message>
</context>
<context>
    <name>QnNewDWPtzController</name>
    <message>
        <location filename="../src/plugins/resource/digitalwatchdog/newdw_ptz_controller.cpp" line="111"/>
        <location filename="../src/plugins/resource/digitalwatchdog/newdw_ptz_controller.cpp" line="155"/>
        <source>Preset #</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>QnPropertyStorage</name>
    <message>
        <location filename="../src/utils/common/property_storage.cpp" line="278"/>
        <source>Invalid value for &apos;%1&apos; argument - expected %2, provided &apos;%3&apos;.</source>
        <translation>Неверное значение параметра %1. Ожидается %2, введено %3.</translation>
    </message>
</context>
<context>
    <name>QnSignHelper</name>
    <message>
        <location filename="../src/export/sign_helper.cpp" line="67"/>
        <location filename="../src/export/sign_helper.cpp" line="712"/>
        <source>Unknown</source>
        <translation>Неизвестно</translation>
    </message>
    <message>
        <location filename="../src/export/sign_helper.cpp" line="70"/>
        <source>Trial license</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/export/sign_helper.cpp" line="262"/>
        <source>Hardware ID: </source>
        <translation>Аппаратный идентификатор: </translation>
    </message>
    <message>
        <location filename="../src/export/sign_helper.cpp" line="716"/>
        <source>FREE license</source>
        <translation>Бесплатная лицензия</translation>
    </message>
    <message>
        <location filename="../src/export/sign_helper.cpp" line="263"/>
        <source>Licensed to: </source>
        <translation>Лицензировано для:</translation>
    </message>
    <message>
        <location filename="../src/export/sign_helper.cpp" line="264"/>
        <source>Watermark: </source>
        <translation>Водяной знак: </translation>
    </message>
</context>
<context>
    <name>QnStreamQualityStrings</name>
    <message>
        <location filename="../src/core/resource/media_resource.cpp" line="19"/>
        <source>Lowest</source>
        <translation>Наихудшее</translation>
    </message>
    <message>
        <location filename="../src/core/resource/media_resource.cpp" line="20"/>
        <source>Low</source>
        <translation>Низкое</translation>
    </message>
    <message>
        <location filename="../src/core/resource/media_resource.cpp" line="22"/>
        <source>High</source>
        <translation>Высокое</translation>
    </message>
    <message>
        <location filename="../src/core/resource/media_resource.cpp" line="24"/>
        <source>Preset</source>
        <translation>Предустановленное</translation>
    </message>
    <message>
        <location filename="../src/core/resource/media_resource.cpp" line="25"/>
        <source>Undefined</source>
        <translation>Неопределено</translation>
    </message>
    <message>
        <location filename="../src/core/resource/media_resource.cpp" line="21"/>
        <source>Medium</source>
        <translation>Среднее</translation>
    </message>
    <message>
        <location filename="../src/core/resource/media_resource.cpp" line="23"/>
        <source>Best</source>
        <translation>Лучшее</translation>
    </message>
    <message>
        <location filename="../src/core/resource/media_resource.cpp" line="37"/>
        <source>Lst</source>
        <extracomment>Short for &apos;Lowest&apos;</extracomment>
        <translation>Наих</translation>
    </message>
    <message>
        <location filename="../src/core/resource/media_resource.cpp" line="40"/>
        <source>Lo</source>
        <extracomment>Short for &apos;Low&apos;</extracomment>
        <translation>Низк</translation>
    </message>
    <message>
        <location filename="../src/core/resource/media_resource.cpp" line="43"/>
        <source>Me</source>
        <extracomment>Short for &apos;Medium&apos;</extracomment>
        <translation>Средн</translation>
    </message>
    <message>
        <location filename="../src/core/resource/media_resource.cpp" line="46"/>
        <source>Hi</source>
        <extracomment>Short for &apos;High&apos;</extracomment>
        <translation>Выс</translation>
    </message>
    <message>
        <location filename="../src/core/resource/media_resource.cpp" line="49"/>
        <source>Bst</source>
        <extracomment>Short for &apos;Best&apos;</extracomment>
        <translation>Лучш</translation>
    </message>
    <message>
        <location filename="../src/core/resource/media_resource.cpp" line="52"/>
        <source>Ps</source>
        <extracomment>Short for &apos;Preset&apos;</extracomment>
        <translation>Пред</translation>
    </message>
    <message>
        <location filename="../src/core/resource/media_resource.cpp" line="55"/>
        <source>-</source>
        <extracomment>Short for &apos;Undefined&apos;</extracomment>
        <translation>-</translation>
    </message>
</context>
<context>
    <name>QnStreamRecorder</name>
    <message>
        <location filename="../src/recording/stream_recorder.cpp" line="52"/>
        <source>Corresponding container in FFMPEG library was not found.</source>
        <translation>Контейнер  не был найден в библиотеке FFMPEG.</translation>
    </message>
    <message>
        <location filename="../src/recording/stream_recorder.cpp" line="53"/>
        <source>Could not create output file for video recording.</source>
        <translation>Невозможно создать файл для записи видео.</translation>
    </message>
    <message>
        <location filename="../src/recording/stream_recorder.cpp" line="54"/>
        <source>Could not allocate output stream for recording.</source>
        <translation>Невозможно создать буфер для записи потока.</translation>
    </message>
    <message>
        <location filename="../src/recording/stream_recorder.cpp" line="55"/>
        <source>Could not allocate output audio stream.</source>
        <translation>Невозможно создать буфер для звукового  потока.</translation>
    </message>
    <message>
        <location filename="../src/recording/stream_recorder.cpp" line="56"/>
        <source>Invalid audio codec information.</source>
        <translation>Неверная информация о звуковом кодеке.</translation>
    </message>
    <message>
        <location filename="../src/recording/stream_recorder.cpp" line="57"/>
        <source>Video or audio codec is incompatible with the selected format.</source>
        <translation>Видео или аудио кодек несовместим с выбранным форматом.</translation>
    </message>
    <message>
        <location filename="../src/recording/stream_recorder.cpp" line="795"/>
        <source>Error during watermark generation for file &apos;%1&apos;.</source>
        <translation>Ошибка при генерации водяного знака для файла %1.</translation>
    </message>
</context>
<context>
    <name>QnSystemHealthStringsHelper</name>
    <message>
        <location filename="../src/health/system_health.cpp" line="8"/>
        <source>No licenses</source>
        <translation>Нет лицензий</translation>
    </message>
    <message>
        <location filename="../src/health/system_health.cpp" line="6"/>
        <source>Email address is not set</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/health/system_health.cpp" line="10"/>
        <source>Email server is not set</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/health/system_health.cpp" line="12"/>
        <source>Some users have not set their email addresses</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/health/system_health.cpp" line="14"/>
        <source>Connection to server lost</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/health/system_health.cpp" line="16"/>
        <source>Select server for others to synchronise time with</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/health/system_health.cpp" line="18"/>
        <source>Error while sending email</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/health/system_health.cpp" line="20"/>
        <source>Storages are full</source>
        <translation>Хранилища заполнены</translation>
    </message>
    <message>
        <location filename="../src/health/system_health.cpp" line="22"/>
        <source>Storages are not configured</source>
        <translation>Хранилища не настроены</translation>
    </message>
    <message>
        <location filename="../src/health/system_health.cpp" line="24"/>
        <source>Rebuilding archive index is completed.</source>
        <translation>Восстановление архива завершено.</translation>
    </message>
    <message>
        <location filename="../src/health/system_health.cpp" line="35"/>
        <source>Email address is not set for user %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/health/system_health.cpp" line="45"/>
        <source>Email address is not set.
You cannot receive system notifications via email.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/health/system_health.cpp" line="47"/>
        <source>Email server is not set.
You cannot receive system notifications via email.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/health/system_health.cpp" line="49"/>
        <source>Some users have not set their email addresses.
They cannot receive system notifications via email.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/health/system_health.cpp" line="51"/>
        <source>Multiple servers have different time and correct time could not be detected automatically.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/health/system_health.cpp" line="53"/>
        <source>Storages are full on the following Server:
%1.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/health/system_health.cpp" line="55"/>
        <source>Storages are not configured on the following Server:
%1.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/health/system_health.cpp" line="59"/>
        <source>Rebuilding archive index is completed on the following Server:
%1.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/health/system_health.cpp" line="57"/>
        <source>You have no licenses.
You cannot record video from cameras.</source>
        <translation>У вас нет лицензий.
Вы не можете записывать видео с камер.</translation>
    </message>
</context>
<context>
    <name>QnTCPConnectionProcessor</name>
    <message>
        <location filename="../src/utils/network/tcp_connection_processor.cpp" line="238"/>
        <source>OK</source>
        <translation>ОК</translation>
    </message>
    <message>
        <location filename="../src/utils/network/tcp_connection_processor.cpp" line="240"/>
        <source>Not Found</source>
        <translation>Не найдено</translation>
    </message>
    <message>
        <location filename="../src/utils/network/tcp_connection_processor.cpp" line="242"/>
        <source>Not Implemented</source>
        <translation>Не реализовано</translation>
    </message>
    <message>
        <location filename="../src/utils/network/tcp_connection_processor.cpp" line="244"/>
        <source>Unsupported Transport</source>
        <translation>Неподдерживаемый транспорт</translation>
    </message>
    <message>
        <location filename="../src/utils/network/tcp_connection_processor.cpp" line="246"/>
        <source>Internal Server Error</source>
        <translation>Внутренняя ошибка сервера</translation>
    </message>
    <message>
        <location filename="../src/utils/network/tcp_connection_processor.cpp" line="248"/>
        <source>Invalid Parameter</source>
        <translation>Некорректный параметр</translation>
    </message>
</context>
<context>
    <name>QnTranscoder</name>
    <message>
        <location filename="../src/transcoding/transcoder.cpp" line="272"/>
        <source>OpenCL transcoding is not implemented.</source>
        <translation>OpenCL кодирование не реализовано.</translation>
    </message>
    <message>
        <location filename="../src/transcoding/transcoder.cpp" line="275"/>
        <source>Unknown transcoding method.</source>
        <translation>Неизвестный метод перекодирования.</translation>
    </message>
    <message>
        <location filename="../src/transcoding/transcoder.cpp" line="299"/>
        <source>OpenCLTranscode is not implemented</source>
        <translation>OpenCLTranscode не реализован</translation>
    </message>
    <message>
        <location filename="../src/transcoding/transcoder.cpp" line="302"/>
        <source>Unknown Transcode Method</source>
        <translation>Неизвестный метод перекодирования</translation>
    </message>
</context>
<context>
    <name>QnTranslationListModel</name>
    <message>
        <location filename="../src/translation/translation_list_model.cpp" line="66"/>
        <source>%1 (built-in)</source>
        <translation>%1 (встроенный)</translation>
    </message>
    <message>
        <location filename="../src/translation/translation_list_model.cpp" line="68"/>
        <source>%1 (external)</source>
        <translation>%1 (внешний)</translation>
    </message>
</context>
</TS>
