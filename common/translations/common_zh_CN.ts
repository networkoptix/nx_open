<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.0" language="zh_CN" sourcelanguage="en">
<context>
    <name>Language</name>
    <message>
        <location filename="../src/translation/translation_manager.cpp" line="153"/>
        <source>Language Name</source>
        <extracomment>Language name that will be displayed to user. Must not be empty.</extracomment>
        <translation>简体中文</translation>
    </message>
    <message>
        <location filename="../src/translation/translation_manager.cpp" line="156"/>
        <source>Locale Code</source>
        <extracomment>Internal. Please don&apos;t change existing translation.</extracomment>
        <translation>zh_CN</translation>
    </message>
</context>
<context>
    <name>Qee::Evaluator</name>
    <message>
        <location filename="../src/utils/common/evaluator.cpp" line="722"/>
        <source>Invalid stack size after program evaluation: %1.</source>
        <translatorcomment>堆栈</translatorcomment>
        <translation> 程序运行后返回无效堆栈大小%1.</translation>
    </message>
    <message>
        <location filename="../src/utils/common/evaluator.cpp" line="764"/>
        <source>Could not deduce result type for operation %1(&apos;%2&apos;, &apos;%3&apos;).</source>
        <translation>无法返回结果类型%1(%2, %3).</translation>
    </message>
    <message>
        <location filename="../src/utils/common/evaluator.cpp" line="792"/>
        <source>Invalid parameter type for operation %1(&apos;%2&apos;, &apos;%2&apos;)</source>
        <translation> 无效的参数类型%1(%2,%2)</translation>
    </message>
    <message>
        <location filename="../src/utils/common/evaluator.cpp" line="802"/>
        <source>Could not deduce arithmetic supertype for type &apos;%1&apos;.</source>
        <translation>无法得出超类型数据%1.</translation>
    </message>
    <message>
        <location filename="../src/utils/common/evaluator.cpp" line="824"/>
        <source>Invalid parameter type for operation %1(&apos;%2&apos;)</source>
        <translation>无效参数%1(%2)</translation>
    </message>
    <message>
        <location filename="../src/utils/common/evaluator.cpp" line="832"/>
        <source>Argument number for %1 instruction has invalid type &apos;%2&apos;.</source>
        <translation>参数 %1 无效指令类型 %2.</translation>
    </message>
    <message>
        <location filename="../src/utils/common/evaluator.cpp" line="836"/>
        <source>Argument number for %1 instruction is invalid.</source>
        <translation>参数 %1 指令无效.</translation>
    </message>
    <message>
        <location filename="../src/utils/common/evaluator.cpp" line="841"/>
        <source>Stack underflow during execution of %1 instruction.</source>
        <translation>执行堆栈%1 指令.</translation>
    </message>
    <message>
        <location filename="../src/utils/common/evaluator.cpp" line="845"/>
        <source>Function name for %1 instruction has invalid type &apos;%2&apos;.</source>
        <translation>功能名称 %1 指令类型无效 %2.</translation>
    </message>
    <message>
        <location filename="../src/utils/common/evaluator.cpp" line="856"/>
        <source>Function or variable &apos;%1&apos; is not defined.</source>
        <translation>功能或是变量 %1 无法定义.</translation>
    </message>
    <message>
        <location filename="../src/utils/common/evaluator.cpp" line="862"/>
        <source>Variable &apos;%1&apos; is not a function and cannot be called.</source>
        <translation>变量 %1 不是一个函数并且不能被返回.</translation>
    </message>
    <message>
        <location filename="../src/utils/common/evaluator.cpp" line="874"/>
        <source>Stack underflow during program evaluation.</source>
        <translation>在程序执行时出现站堆栈溢出</translation>
    </message>
</context>
<context>
    <name>Qee::Lexer</name>
    <message>
        <location filename="../src/utils/common/evaluator.cpp" line="113"/>
        <source>Unexpected symbol &apos;%1&apos; at position %2.</source>
        <translation>未知符号&apos;％1&apos;在位置％2。</translation>
    </message>
</context>
<context>
    <name>Qee::ParameterPack</name>
    <message>
        <location filename="../src/utils/common/evaluator.h" line="185"/>
        <location filename="../src/utils/common/evaluator.h" line="192"/>
        <source>Parameter %2 is not specified for function &apos;%1&apos;.</source>
        <translation>参数％2不是函数&apos;％1&apos;指定</translation>
    </message>
    <message>
        <location filename="../src/utils/common/evaluator.h" line="195"/>
        <source>Parameter %2 of function &apos;%1&apos; is of type &apos;%3&apos;, but type &apos;%4&apos; was expected.</source>
        <translation>功能参数 &apos;%1&apos; 的%2&apos;&apos;是&apos;%3&apos;&apos;类型, 但类型&apos;%4&apos;是&apos;预期的.</translation>
    </message>
    <message>
        <location filename="../src/utils/common/evaluator.h" line="201"/>
        <source>Function &apos;%1&apos; is expected to have %3 arguments, %2 provided.</source>
        <translation>功能 &apos;%1&apos;是预期有%3的论据, %2提供.</translation>
    </message>
    <message>
        <location filename="../src/utils/common/evaluator.h" line="206"/>
        <source>Function &apos;%1&apos; is expected to have %3-%4 arguments, %2 provided.</source>
        <translation>功能 &apos;%1&apos;是预期有%3-%4的论据, %2提供.</translation>
    </message>
</context>
<context>
    <name>Qee::Parser</name>
    <message>
        <location filename="../src/utils/common/evaluator.cpp" line="179"/>
        <source>Unexpected token %1 (&apos;%2&apos;) at position %3.</source>
        <translation>未知符号%1 (&apos;%2&apos;) 在位置 %3.</translation>
    </message>
    <message>
        <location filename="../src/utils/common/evaluator.cpp" line="236"/>
        <source>Invalid color constant &apos;%1&apos;.</source>
        <translation>&apos;%1&apos;无效颜色对比.</translation>
    </message>
    <message>
        <location filename="../src/utils/common/evaluator.cpp" line="281"/>
        <source>Invalid number constant &apos;%1&apos;.</source>
        <translation>&apos;%1&apos;无效数字对比.</translation>
    </message>
</context>
<context>
    <name>QnAbstractStorageResource</name>
    <message>
        <location filename="../src/core/resource/abstract_storage_resource.cpp" line="120"/>
        <source>Windows Network Shared Resource</source>
        <translation>Windows 网络共享资源</translation>
    </message>
    <message>
        <location filename="../src/core/resource/abstract_storage_resource.cpp" line="121"/>
        <source>\\&lt;Computer Name&gt;\&lt;Folder&gt;</source>
        <translation>\\&lt;计算机名&gt;\&lt;文件夹&gt;</translation>
    </message>
    <message>
        <location filename="../src/core/resource/abstract_storage_resource.cpp" line="125"/>
        <source>Coldstore Network Storage</source>
        <translation>NAS网络存储</translation>
    </message>
    <message>
        <location filename="../src/core/resource/abstract_storage_resource.cpp" line="126"/>
        <source>coldstore://&lt;Address&gt;</source>
        <translation>NAS://&lt;地址&gt;</translation>
    </message>
</context>
<context>
    <name>QnBusinessStringsHelper</name>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="64"/>
        <source>User Defined (%1)</source>
        <translation>用户定义 (%1)</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="68"/>
        <source>Motion on Camera</source>
        <translation>摄像机移动侦测</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="69"/>
        <source>Input Signal on Camera</source>
        <translation>摄像机报警信号输入</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="70"/>
        <source>Camera Disconnected</source>
        <translation>摄像机无法连接</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="71"/>
        <source>Storage Failure</source>
        <translation>存储故障</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="72"/>
        <source>Network Issue</source>
        <translation>网络问题</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="73"/>
        <source>Camera IP Conflict</source>
        <translation>摄像机IP冲突</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="95"/>
        <source>Camera %1 was disconnected</source>
        <translation>摄像机%1无法连接</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="98"/>
        <source>Input on %1</source>
        <translation>%1 报警输入</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="101"/>
        <source>Motion on %1</source>
        <translation>%1侦测到移动</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="104"/>
        <source>Storage Failure at %1</source>
        <translation>%1存储故障</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="107"/>
        <source>Network Issue at %1</source>
        <translation>%1网络问题</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="113"/>
        <source>Camera IP Conflict at %1</source>
        <translation>%1摄像机IP冲突</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="45"/>
        <source>Camera output</source>
        <translation>摄像机报警输出</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="46"/>
        <source>Camera output for 30 sec</source>
        <translation>摄相机报警输出30秒</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="47"/>
        <source>Bookmark</source>
        <translation>标记</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="48"/>
        <source>Camera recording</source>
        <translation>摄像机录像</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="49"/>
        <source>Panic recording</source>
        <translation>危机录像</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="51"/>
        <source>Write to log</source>
        <translation>写入日志</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="52"/>
        <source>Show notification</source>
        <translation>显示通知</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="54"/>
        <source>Play sound</source>
        <translation>播放声音</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="53"/>
        <source>Repeat sound</source>
        <translation>循环播放声音</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="50"/>
        <source>Send email</source>
        <translation>发送邮件</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="55"/>
        <source>Speak</source>
        <translation>说话</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="56"/>
        <source>Unknown (%1)</source>
        <translation>未知 (%1)</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="78"/>
        <source>Any Camera Issue</source>
        <translation>任何摄像机问题</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="79"/>
        <source>Any Server Issue</source>
        <translation>任何服务器问题</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="80"/>
        <source>Any Event</source>
        <translation>任何事件</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="92"/>
        <source>Undefined event has occurred on %1</source>
        <translation>未定义事件在 %1发生</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="74"/>
        <source>Server Failure</source>
        <translation>服务器失效</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="75"/>
        <source>Server Conflict</source>
        <translation>服务器冲突</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="76"/>
        <source>Server Started</source>
        <translation>服务器启动</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="77"/>
        <source>License Issue</source>
        <translation>授权问题</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="110"/>
        <source>Server &quot;%1&quot; Failure</source>
        <translation>服务器 &quot;%1&quot; 失效</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="116"/>
        <source>Server &quot;%1&quot; Conflict</source>
        <translation>服务器 &quot;%1&quot; 冲突</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="119"/>
        <source>Server &quot;%1&quot; Started</source>
        <translation>服务器 &quot;%1&quot;已开启</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="121"/>
        <source>Server &quot;%1&quot; had license issue</source>
        <translation>服务器 &quot;%1&quot; 出现授权问题</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="126"/>
        <source>Unknown event has occurred</source>
        <translation>发生未知事件</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="144"/>
        <source>Event: %1</source>
        <translation>事件: %1</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="147"/>
        <source>Source: %1</source>
        <translation>来源: %1</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="151"/>
        <source>Url: %1</source>
        <translation>网址: %1</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="190"/>
        <source>Input port: %1</source>
        <translation>报警输入端口: %1</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="198"/>
        <source>Reason: %1</source>
        <translation>原因: %1</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="202"/>
        <source>Conflict address: %1</source>
        <translation>冲突IP地址:: %1</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="207"/>
        <location filename="../src/business/business_strings_helper.cpp" line="223"/>
        <source>Camera #%1 MAC: %2</source>
        <translation>摄像机#%1 MAC: %2</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="219"/>
        <source>Conflicting Server #%1: %2</source>
        <translation>服务器冲突 #%1: %2</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="370"/>
        <source>Connection to camera (primary stream) was unexpectedly closed.</source>
        <translation>无法连接摄像机(主码流)</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="372"/>
        <source>Connection to camera (secondary stream) was unexpectedly closed.</source>
        <translation>无法连接摄像机(次码流)</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="408"/>
        <source>Recording on %n camera(s) is disabled: </source>
        <translation>在 %n 摄像机录像被禁用</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="325"/>
        <source>%2 %1</source>
        <comment>%1 means time, %2 means date</comment>
        <translatorcomment>%1 是时间, %2 是日期</translatorcomment>
        <translation>%2 %1</translation>
    </message>
    <message numerus="yes">
        <location filename="../src/business/business_strings_helper.cpp" line="329"/>
        <source>%n times, first: %2 %1</source>
        <comment>%1 means time, %2 means date</comment>
        <translatorcomment>%1 是时间, %2 是日期</translatorcomment>
        <translation>
            <numerusform>%n次, 首次: %2 %1</numerusform>
        </translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="340"/>
        <source>Time: %1 on %2</source>
        <comment>%1 means time, %2 means date</comment>
        <translatorcomment>%1 是时间, %2 是日期</translatorcomment>
        <translation>%2 时间: %1 </translation>
    </message>
    <message numerus="yes">
        <location filename="../src/business/business_strings_helper.cpp" line="344"/>
        <source>First occurrence: %1 on %2 (%n times total)</source>
        <comment>%1 means time, %2 means date</comment>
        <translatorcomment>%1 是时间, %2 是日期</translatorcomment>
        <translation>
            <numerusform>第一次出现: %1 在 %2 (%n 总次数）</numerusform>
        </translation>
    </message>
    <message numerus="yes">
        <location filename="../src/business/business_strings_helper.cpp" line="364"/>
        <source>No video frame received during last %n seconds.</source>
        <translation>
            <numerusform>在过去的 %n 秒中没有接收到视频帧.</numerusform>
        </translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="378"/>
        <source>RTP packet loss detected, prev seq.=%1 next seq.=%2.</source>
        <translation>检测到RTP包丢失。前个序列号=%1，下个序列号=%2.</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="380"/>
        <source>RTP packet loss detected.</source>
        <translation>检测到RTP包丢失.</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="393"/>
        <source>I/O error has occurred at %1.</source>
        <translation>I/0错误发生在 %1</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="398"/>
        <source>Not enough HDD/SSD speed for recording to %1.</source>
        <translation>硬盘写入速度不足以录像至%1.。</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="403"/>
        <source>HDD/SSD disk %1 is full. Disk contains too much data that is not managed by VMS.</source>
        <translation>HDD / SSD磁盘%1已满.磁盘包含太多不是由VMS管理的数据.</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="384"/>
        <source>Server terminated.</source>
        <translation>服务器停止.</translation>
    </message>
    <message>
        <location filename="../src/business/business_strings_helper.cpp" line="388"/>
        <source>Server started after crash.</source>
        <translation>服务器停止后启动.</translation>
    </message>
</context>
<context>
    <name>QnCameraDiagnosticsErrorCodeStrings</name>
    <message>
        <location filename="../src/utils/camera/camera_diagnostics.cpp" line="130"/>
        <source>(unknown)</source>
        <translation>(未知错误)</translation>
    </message>
    <message>
        <location filename="../src/utils/camera/camera_diagnostics.cpp" line="27"/>
        <source>OK</source>
        <translation>确定</translation>
    </message>
    <message>
        <location filename="../src/utils/camera/camera_diagnostics.cpp" line="31"/>
        <source>Server %1 is not available.
 Check that Server is up and running.</source>
        <translation>服务器 %1 不可用，
请检查服务器开启并运行。</translation>
    </message>
    <message>
        <location filename="../src/utils/camera/camera_diagnostics.cpp" line="35"/>
        <source>Received bad response from Server %1: &quot;%2&quot;.
 Check if Server is up and has the proper version.</source>
        <translation>从服务器获取错误返回 %1: &quot;%2&quot;.
 请检查服务器是否开启并运行正确版本。</translation>
    </message>
    <message>
        <location filename="../src/utils/camera/camera_diagnostics.cpp" line="39"/>
        <source>Cannot connect to http port %1.
 Make sure the camera is plugged into the network.</source>
        <translation>无法连接到HTTP端口%1。
确认摄像机连上网络。</translation>
    </message>
    <message>
        <location filename="../src/utils/camera/camera_diagnostics.cpp" line="43"/>
        <source>Cannot open media url %1. Failed to connect to media port %2.
 Make sure port %2 is accessible (e.g. forwarded). Please try to reboot the camera, then restore factory defaults on the web-page.</source>
        <translation>无法连接到媒体 url %1。无法连接到媒体端口 %2。
确认访问端口 %2可以存取(转发等)。请尝试重新启动摄像机，然后通过网页恢复出厂默认。</translation>
    </message>
    <message>
        <location filename="../src/utils/camera/camera_diagnostics.cpp" line="48"/>
        <source>Cannot open media url %1. Connection to port %2 was closed unexpectedly.
 Make sure the camera is plugged into the network. Try to reboot the camera.</source>
        <translation>无法连接到媒体 url %1。%2 端口上的连接被意外关闭。
确认摄像机连上网络，尝试重新启动摄像机。</translation>
    </message>
    <message>
        <location filename="../src/utils/camera/camera_diagnostics.cpp" line="53"/>
        <source>Could not parse camera response. Url %1, request name %2.
 Please try to reboot the camera, then restore factory defaults on the web-page. Finally, try to update firmware. If the problem persists, please contact support.</source>
        <translation>无法解析摄像机响应。 URL%1，要求名称 %2。 
 请尝试重新启动摄像机，然后通过网页恢复出厂默认。最后，尝试更新固件。如果问题仍然存在，请联系技术支持.</translation>
    </message>
    <message>
        <location filename="../src/utils/camera/camera_diagnostics.cpp" line="58"/>
        <source>No supported media tracks at url %1.
 Please try to reboot the camera, then restore factory defaults on the web-page. Finally, try to update firmware. If the problem persists, please contact support.</source>
        <translation>在URL%1上出现不支持的视频流。 
 请尝试重新启动摄像机，然后通过网页恢复出厂默认设置。最后，尝试更新固件。如果问题仍然存在，请联系技术支持。</translation>
    </message>
    <message>
        <location filename="../src/utils/camera/camera_diagnostics.cpp" line="63"/>
        <source>Not authorized. Url %1.</source>
        <translation>未经授权。Url %1.</translation>
    </message>
    <message>
        <location filename="../src/utils/camera/camera_diagnostics.cpp" line="67"/>
        <source>Cannot open media url %1. Unsupported media protocol %2.
 Please try to restore factory defaults on the web-page. Finally, try to update firmware. If the problem persists, please contact support.</source>
        <translation>无法打开媒体URL %1。不支持的媒体协议 %2。 
 请尝试通过网页恢复出厂设置。最后，尝试更新固件。如果问题仍然存在，请联系技术支持。</translation>
    </message>
    <message>
        <location filename="../src/utils/camera/camera_diagnostics.cpp" line="72"/>
        <source>Failed to configure parameter %1.
 First, try to turn on recording (if it&apos;s off) and decrease fps in camera settings. If it doesn&apos;t help, restore factory defaults on the camera web-page. If the problem persists, please contact support.</source>
        <translation>无法配置参数%1。 
 首先，尝试打开录音（如果它是关闭），并在摄像机中降低FPS(每秒的帧速率)。如果仍未解决，请恢复摄像机网页上的出厂默认值。如果问题仍然存在，请联系技术支持。</translation>
    </message>
    <message>
        <location filename="../src/utils/camera/camera_diagnostics.cpp" line="77"/>
        <source>Camera request &quot;%1&quot; failed with error &quot;%2&quot;.
 Please try to reboot the camera, then restore factory defaults on the web-page. Finally, try to update firmware. If the problem persists, please contact support.</source>
        <translation>相机的请求“％=%1”失败，错误“%2”。 
 请尝试重新启动摄像机，然后通过网页恢复出厂默认。最后，尝试更新固件。如果问题仍然存在，请联系技术支持。</translation>
    </message>
    <message>
        <location filename="../src/utils/camera/camera_diagnostics.cpp" line="82"/>
        <source>Unknown Camera Issue.
 Please contact support.</source>
        <translation>未知摄像机问题。 
 请联系技术支持团队.</translation>
    </message>
    <message>
        <location filename="../src/utils/camera/camera_diagnostics.cpp" line="86"/>
        <source>An input/output error has occurred. OS message: &quot;%1&quot;.
 Make sure the camera is plugged into the network. Try to reboot the camera.</source>
        <translation>发生输入/输出错误。 OS消息：“%1”。 
 确保摄像机已连接到网络。尝试重新启动摄像机。</translation>
    </message>
    <message>
        <location filename="../src/utils/camera/camera_diagnostics.cpp" line="93"/>
        <source>Invalid data was received from the camera: %1.</source>
        <translation>从摄像机获取到无效数据：%1</translation>
    </message>
    <message>
        <location filename="../src/utils/camera/camera_diagnostics.cpp" line="96"/>
        <source>Too many media errors. Please open camera issues dialog for more details.</source>
        <translation>视频数据错误，请打开摄像机检测窗口获取更多细节信息。</translation>
    </message>
    <message>
        <location filename="../src/utils/camera/camera_diagnostics.cpp" line="99"/>
        <source>Media stream is opened but no media data was received.</source>
        <translation>媒体流被打开，但没有收到媒体数据。</translation>
    </message>
    <message>
        <location filename="../src/utils/camera/camera_diagnostics.cpp" line="102"/>
        <source>Camera initialization process in progress</source>
        <translation>摄像机正在进行初始化</translation>
    </message>
    <message>
        <location filename="../src/utils/camera/camera_diagnostics.cpp" line="117"/>
        <source>

Parameters: </source>
        <translation>

参数：</translation>
    </message>
    <message>
        <location filename="../src/utils/camera/camera_diagnostics.cpp" line="89"/>
        <source>Server has been stopped.</source>
        <translation>服务器已停止。</translation>
    </message>
    <message>
        <location filename="../src/utils/camera/camera_diagnostics.cpp" line="115"/>
        <source>Unknown error. Please contact support.</source>
        <translation>未知错误，请联系技术支持团队。</translation>
    </message>
</context>
<context>
    <name>QnCommandLineParser</name>
    <message>
        <location filename="../src/utils/common/command_line_parser.cpp" line="154"/>
        <source>No value provided for the &apos;%1&apos; argument.</source>
        <translation>无法提供有效值 &apos;%1&apos;</translation>
    </message>
    <message>
        <location filename="../src/utils/common/command_line_parser.cpp" line="166"/>
        <source>Invalid value for &apos;%1&apos; argument - expected %2, provided &apos;%3&apos;.</source>
        <translation>&apos;%1&apos;无效值－期望%2, 提供%3。</translation>
    </message>
</context>
<context>
    <name>QnEnvironment</name>
    <message>
        <location filename="../src/utils/common/environment.cpp" line="58"/>
        <source>Launching Windows Explorer failed</source>
        <translation>启动Windows浏览器失败</translation>
    </message>
    <message>
        <location filename="../src/utils/common/environment.cpp" line="59"/>
        <source>Could not find explorer.exe in path to launch Windows Explorer.</source>
        <translation>启动Windows浏览器时无法找到explorer.exe。</translation>
    </message>
</context>
<context>
    <name>QnFfmpegAudioTranscoder</name>
    <message>
        <location filename="../src/transcoding/ffmpeg_audio_transcoder.cpp" line="53"/>
        <source>Audio context was not specified.</source>
        <translation>未指定音频内容</translation>
    </message>
    <message>
        <location filename="../src/transcoding/ffmpeg_audio_transcoder.cpp" line="67"/>
        <source>Could not find encoder for codec %1.</source>
        <translation>%1无法发现音频编码器.</translation>
    </message>
    <message>
        <location filename="../src/transcoding/ffmpeg_audio_transcoder.cpp" line="94"/>
        <source>Could not initialize audio encoder.</source>
        <translation>无法初始化音频编码器.</translation>
    </message>
    <message>
        <location filename="../src/transcoding/ffmpeg_audio_transcoder.cpp" line="103"/>
        <source>Could not initialize audio decoder.</source>
        <translation>无法初始化音频解码器.</translation>
    </message>
</context>
<context>
    <name>QnFfmpegTranscoder</name>
    <message>
        <location filename="../src/transcoding/ffmpeg_transcoder.cpp" line="176"/>
        <source>Container %1 was not found in FFMPEG library.</source>
        <translation>容器%1不是在FFMPEG库中找到。</translation>
    </message>
    <message>
        <location filename="../src/transcoding/ffmpeg_transcoder.cpp" line="185"/>
        <source>Could not create output context for format %1.</source>
        <translation>无法创建输出格式 %1.</translation>
    </message>
    <message>
        <location filename="../src/transcoding/ffmpeg_transcoder.cpp" line="204"/>
        <location filename="../src/transcoding/ffmpeg_transcoder.cpp" line="284"/>
        <source>Could not allocate output stream for recording.</source>
        <translation>无法为录像分配输出流。</translation>
    </message>
    <message>
        <location filename="../src/transcoding/ffmpeg_transcoder.cpp" line="245"/>
        <source>Could not perform direct stream copy because frame size is undefined.</source>
        <translation>不能执行直接视频流复制，原因是帧的大小是不确定的。</translation>
    </message>
    <message>
        <location filename="../src/transcoding/ffmpeg_transcoder.cpp" line="292"/>
        <source>Could not find codec %1.</source>
        <translation>%1无法发现音频编码器.</translation>
    </message>
    <message>
        <location filename="../src/transcoding/ffmpeg_transcoder.cpp" line="330"/>
        <source>Video or audio codec is incompatible with container %1.</source>
        <translation>视频或音频编解码器与%1选定格式不兼容。</translation>
    </message>
</context>
<context>
    <name>QnFfmpegVideoTranscoder</name>
    <message>
        <location filename="../src/transcoding/ffmpeg_video_transcoder.cpp" line="59"/>
        <source>Could not find encoder for codec %1.</source>
        <translation>%1无法发现音频编码器.</translation>
    </message>
    <message>
        <location filename="../src/transcoding/ffmpeg_video_transcoder.cpp" line="106"/>
        <source>Could not initialize video encoder.</source>
        <translation>无法初始化视频编码器</translation>
    </message>
</context>
<context>
    <name>QnLicense</name>
    <message>
        <location filename="../src/licensing/license.cpp" line="151"/>
        <source>Trial</source>
        <translation>测试版</translation>
    </message>
    <message>
        <location filename="../src/licensing/license.cpp" line="152"/>
        <source>Analog</source>
        <translation>模拟</translation>
    </message>
    <message>
        <location filename="../src/licensing/license.cpp" line="153"/>
        <source>Professional</source>
        <translation>专业版</translation>
    </message>
    <message>
        <location filename="../src/licensing/license.cpp" line="154"/>
        <source>Edge</source>
        <translation>edge</translation>
    </message>
    <message>
        <location filename="../src/licensing/license.cpp" line="155"/>
        <source>Vmax</source>
        <translation>Vmax</translation>
    </message>
    <message>
        <location filename="../src/licensing/license.cpp" line="156"/>
        <source>Analog encoder</source>
        <translation>模拟编码器</translation>
    </message>
    <message>
        <location filename="../src/licensing/license.cpp" line="157"/>
        <source>Video Wall</source>
        <translation>电视墙</translation>
    </message>
    <message>
        <location filename="../src/licensing/license.cpp" line="170"/>
        <source>Trial licenses</source>
        <translation>测试授权</translation>
    </message>
    <message>
        <location filename="../src/licensing/license.cpp" line="171"/>
        <source>Analog licenses</source>
        <translation>模拟授权</translation>
    </message>
    <message>
        <location filename="../src/licensing/license.cpp" line="172"/>
        <source>Professional licenses</source>
        <translation>专业版授权</translation>
    </message>
    <message>
        <location filename="../src/licensing/license.cpp" line="173"/>
        <source>Edge licenses</source>
        <translation>edge授权</translation>
    </message>
    <message>
        <location filename="../src/licensing/license.cpp" line="174"/>
        <source>Vmax licenses</source>
        <translation>Vmax许可</translation>
    </message>
    <message>
        <location filename="../src/licensing/license.cpp" line="175"/>
        <source>Analog encoder licenses</source>
        <translation>模拟编码器授权</translation>
    </message>
    <message>
        <location filename="../src/licensing/license.cpp" line="176"/>
        <source>Video Wall licenses</source>
        <translation>电视墙授权</translation>
    </message>
    <message>
        <location filename="../src/licensing/license.cpp" line="343"/>
        <source>Invalid signature</source>
        <translation>无效签名</translation>
    </message>
    <message>
        <location filename="../src/licensing/license.cpp" line="345"/>
        <source>Server with necessary hardware ID is not found</source>
        <translation>服务器所需的硬件ID无法找到</translation>
    </message>
    <message>
        <location filename="../src/licensing/license.cpp" line="347"/>
        <source>Invalid customization</source>
        <translation>无效定制</translation>
    </message>
    <message>
        <location filename="../src/licensing/license.cpp" line="349"/>
        <source>Expired</source>
        <translation>过期</translation>
    </message>
    <message>
        <location filename="../src/licensing/license.cpp" line="351"/>
        <source>Invalid type</source>
        <translation>无效类型</translation>
    </message>
    <message>
        <location filename="../src/licensing/license.cpp" line="353"/>
        <source>Only single license is allowed for this device</source>
        <translation>此设备需要单独授权</translation>
    </message>
    <message>
        <location filename="../src/licensing/license.cpp" line="355"/>
        <source>Unknown error</source>
        <translation>未知错误</translation>
    </message>
</context>
<context>
    <name>QnLicenseUsageHelper</name>
    <message numerus="yes">
        <location filename="../src/utils/license_usage_helper.cpp" line="75"/>
        <source>%n %2 are used out of %1.</source>
        <translation>
            <numerusform>%n %2 在使用 %1.</numerusform>
        </translation>
    </message>
    <message numerus="yes">
        <location filename="../src/utils/license_usage_helper.cpp" line="94"/>
        <source>%n %2 will be used out of %1.</source>
        <translation>
            <numerusform>%n %2 将使用 %1.</numerusform>
        </translation>
    </message>
    <message numerus="yes">
        <location filename="../src/utils/license_usage_helper.cpp" line="112"/>
        <source>Activate %n more %2. </source>
        <translatorcomment>激活 %n 多于 %2. </translatorcomment>
        <translation>
            <numerusform>Activate %n more %2. </numerusform>
        </translation>
    </message>
    <message numerus="yes">
        <location filename="../src/utils/license_usage_helper.cpp" line="114"/>
        <source>%n more %2 will be used. </source>
        <translation>
            <numerusform>%n 多于 %2 将被使用</numerusform>
        </translation>
    </message>
    <message>
        <location filename="../src/utils/license_usage_helper.cpp" line="186"/>
        <source>There was a problem activating your license key. Database error has occurred.</source>
        <translatorcomment>授权码出现问题，发生数据库错误。</translatorcomment>
        <translation>There was a problem activating your license key. Database error has occurred.</translation>
    </message>
    <message>
        <location filename="../src/utils/license_usage_helper.cpp" line="188"/>
        <source>There was a problem activating your license key. Invalid data received. Please contact support team to report issue.</source>
        <translation>授权码出现问题，接收到无效数据，请联系技术支持报告此问题。</translation>
    </message>
    <message>
        <location filename="../src/utils/license_usage_helper.cpp" line="190"/>
        <source>The license key you have entered is invalid. Please check that license key is entered correctly. If problem continues, please contact support team to confirm if license key is valid or to get a valid license key.</source>
        <translation>输入授权无效，请确认输入的授权是否正确。如果问题仍存在，请联系技术支持确认授权码是否有效或获取有效授权码。</translation>
    </message>
    <message>
        <location filename="../src/utils/license_usage_helper.cpp" line="193"/>
        <source>You are trying to activate an incompatible license with your software. Please contact support team to get a valid license key.</source>
        <translation>当前激活的授权码不可用，请联系技术支持获取有效授权码。</translation>
    </message>
    <message>
        <location filename="../src/utils/license_usage_helper.cpp" line="195"/>
        <source>This license key has been previously activated to hardware id {{hwid}} on {{time}}. Please contact support team to get a valid license key.</source>
        <translation>此授权码已在 {{time}}激活过硬件ID{{hwid}} ，请联系技术支持获取有效的授权码。</translation>
    </message>
</context>
<context>
    <name>QnMediaServerResource</name>
    <message>
        <location filename="../src/core/resource/media_server_resource.cpp" line="44"/>
        <source>Server</source>
        <translation>服务器</translation>
    </message>
</context>
<context>
    <name>QnNewDWPtzController</name>
    <message>
        <location filename="../src/plugins/resource/digitalwatchdog/newdw_ptz_controller.cpp" line="111"/>
        <location filename="../src/plugins/resource/digitalwatchdog/newdw_ptz_controller.cpp" line="155"/>
        <source>Preset #</source>
        <translation>预置位 #</translation>
    </message>
</context>
<context>
    <name>QnPropertyStorage</name>
    <message>
        <location filename="../src/utils/common/property_storage.cpp" line="278"/>
        <source>Invalid value for &apos;%1&apos; argument - expected %2, provided &apos;%3&apos;.</source>
        <translation>&apos;%1&apos;无效数值－期望%2, 提供%3。</translation>
    </message>
</context>
<context>
    <name>QnSignHelper</name>
    <message>
        <location filename="../src/export/sign_helper.cpp" line="67"/>
        <location filename="../src/export/sign_helper.cpp" line="712"/>
        <source>Unknown</source>
        <translation>未知</translation>
    </message>
    <message>
        <location filename="../src/export/sign_helper.cpp" line="70"/>
        <source>Trial license</source>
        <translation>试用授权</translation>
    </message>
    <message>
        <location filename="../src/export/sign_helper.cpp" line="262"/>
        <source>Hardware ID: </source>
        <translation>硬件ID:</translation>
    </message>
    <message>
        <location filename="../src/export/sign_helper.cpp" line="716"/>
        <source>FREE license</source>
        <translation>免费许可</translation>
    </message>
    <message>
        <location filename="../src/export/sign_helper.cpp" line="263"/>
        <source>Licensed to: </source>
        <translation>授权给:</translation>
    </message>
    <message>
        <location filename="../src/export/sign_helper.cpp" line="264"/>
        <source>Watermark: </source>
        <translation>水印:</translation>
    </message>
</context>
<context>
    <name>QnStreamQualityStrings</name>
    <message>
        <location filename="../src/core/resource/media_resource.cpp" line="19"/>
        <source>Lowest</source>
        <translation>最低</translation>
    </message>
    <message>
        <location filename="../src/core/resource/media_resource.cpp" line="20"/>
        <source>Low</source>
        <translation>低</translation>
    </message>
    <message>
        <location filename="../src/core/resource/media_resource.cpp" line="22"/>
        <source>High</source>
        <translation>高</translation>
    </message>
    <message>
        <location filename="../src/core/resource/media_resource.cpp" line="24"/>
        <source>Preset</source>
        <translation>预置位</translation>
    </message>
    <message>
        <location filename="../src/core/resource/media_resource.cpp" line="25"/>
        <source>Undefined</source>
        <translation>未定义</translation>
    </message>
    <message>
        <location filename="../src/core/resource/media_resource.cpp" line="21"/>
        <source>Medium</source>
        <translation>中</translation>
    </message>
    <message>
        <location filename="../src/core/resource/media_resource.cpp" line="23"/>
        <source>Best</source>
        <translation>最佳</translation>
    </message>
    <message>
        <location filename="../src/core/resource/media_resource.cpp" line="37"/>
        <source>Lst</source>
        <extracomment>Short for &apos;Lowest&apos;</extracomment>
        <translation>最低</translation>
    </message>
    <message>
        <location filename="../src/core/resource/media_resource.cpp" line="40"/>
        <source>Lo</source>
        <extracomment>Short for &apos;Low&apos;</extracomment>
        <translation>低</translation>
    </message>
    <message>
        <location filename="../src/core/resource/media_resource.cpp" line="43"/>
        <source>Me</source>
        <extracomment>Short for &apos;Medium&apos;</extracomment>
        <translation>中</translation>
    </message>
    <message>
        <location filename="../src/core/resource/media_resource.cpp" line="46"/>
        <source>Hi</source>
        <extracomment>Short for &apos;High&apos;</extracomment>
        <translation>高</translation>
    </message>
    <message>
        <location filename="../src/core/resource/media_resource.cpp" line="49"/>
        <source>Bst</source>
        <extracomment>Short for &apos;Best&apos;</extracomment>
        <translation>最佳</translation>
    </message>
    <message>
        <location filename="../src/core/resource/media_resource.cpp" line="52"/>
        <source>Ps</source>
        <extracomment>Short for &apos;Preset&apos;</extracomment>
        <translation>预置位</translation>
    </message>
    <message>
        <location filename="../src/core/resource/media_resource.cpp" line="55"/>
        <source>-</source>
        <extracomment>Short for &apos;Undefined&apos;</extracomment>
        <translatorcomment>缩写&apos;Undefined&apos;</translatorcomment>
        <translation>-</translation>
    </message>
</context>
<context>
    <name>QnStreamRecorder</name>
    <message>
        <location filename="../src/recording/stream_recorder.cpp" line="52"/>
        <source>Corresponding container in FFMPEG library was not found.</source>
        <translation>未找到FFMPEG库相应的容器。</translation>
    </message>
    <message>
        <location filename="../src/recording/stream_recorder.cpp" line="53"/>
        <source>Could not create output file for video recording.</source>
        <translation>无法为视频录像创建输出文件.</translation>
    </message>
    <message>
        <location filename="../src/recording/stream_recorder.cpp" line="54"/>
        <source>Could not allocate output stream for recording.</source>
        <translation>无法为录像分配输出流。</translation>
    </message>
    <message>
        <location filename="../src/recording/stream_recorder.cpp" line="55"/>
        <source>Could not allocate output audio stream.</source>
        <translation>无法分配输出音频流。</translation>
    </message>
    <message>
        <location filename="../src/recording/stream_recorder.cpp" line="56"/>
        <source>Invalid audio codec information.</source>
        <translation>无效的音频编解码器的信息。</translation>
    </message>
    <message>
        <location filename="../src/recording/stream_recorder.cpp" line="57"/>
        <source>Video or audio codec is incompatible with the selected format.</source>
        <translation>视频或音频编解码器与选定格式不兼容。</translation>
    </message>
    <message>
        <location filename="../src/recording/stream_recorder.cpp" line="795"/>
        <source>Error during watermark generation for file &apos;%1&apos;.</source>
        <translation>为文件&apos;%1&apos;生成水印时发生错误。</translation>
    </message>
</context>
<context>
    <name>QnSystemHealthStringsHelper</name>
    <message>
        <location filename="../src/health/system_health.cpp" line="8"/>
        <source>No licenses</source>
        <translation>无软件许可</translation>
    </message>
    <message>
        <location filename="../src/health/system_health.cpp" line="6"/>
        <source>Email address is not set</source>
        <translation>邮箱地址未设置</translation>
    </message>
    <message>
        <location filename="../src/health/system_health.cpp" line="10"/>
        <source>Email server is not set</source>
        <translation>邮件服务器未设置</translation>
    </message>
    <message>
        <location filename="../src/health/system_health.cpp" line="12"/>
        <source>Some users have not set their email addresses</source>
        <translation>部分用户未设置邮箱地址。</translation>
    </message>
    <message>
        <location filename="../src/health/system_health.cpp" line="14"/>
        <source>Connection to server lost</source>
        <translation>无法连接到服务器</translation>
    </message>
    <message>
        <location filename="../src/health/system_health.cpp" line="16"/>
        <source>Select server for others to synchronise time with</source>
        <translation>请选择服务器进行时间同步</translation>
    </message>
    <message>
        <location filename="../src/health/system_health.cpp" line="18"/>
        <source>Error while sending email</source>
        <translation>发送邮件错误</translation>
    </message>
    <message>
        <location filename="../src/health/system_health.cpp" line="20"/>
        <source>Storages are full</source>
        <translation>存储空间已满</translation>
    </message>
    <message>
        <location filename="../src/health/system_health.cpp" line="22"/>
        <source>Storages are not configured</source>
        <translation>未设置存储空间</translation>
    </message>
    <message>
        <location filename="../src/health/system_health.cpp" line="24"/>
        <source>Rebuilding archive index is completed.</source>
        <translation>重建录像索引完成.</translation>
    </message>
    <message>
        <location filename="../src/health/system_health.cpp" line="35"/>
        <source>Email address is not set for user %1</source>
        <translation>未给用户 %1设置邮箱地址</translation>
    </message>
    <message>
        <location filename="../src/health/system_health.cpp" line="45"/>
        <source>Email address is not set.
You cannot receive system notifications via email.</source>
        <translation>邮件地址未设置，
将无法接收到系统邮件通知</translation>
    </message>
    <message>
        <location filename="../src/health/system_health.cpp" line="47"/>
        <source>Email server is not set.
You cannot receive system notifications via email.</source>
        <translation>邮件服务器未设置
将无法接受到系统邮件通知</translation>
    </message>
    <message>
        <location filename="../src/health/system_health.cpp" line="49"/>
        <source>Some users have not set their email addresses.
They cannot receive system notifications via email.</source>
        <translation>部分用户未设置邮箱地址，
将无法收到系统通知邮件。</translation>
    </message>
    <message>
        <location filename="../src/health/system_health.cpp" line="51"/>
        <source>Multiple servers have different time and correct time could not be detected automatically.</source>
        <translation>多服务器上的时间不一致，无法自动获取到准确时间。</translation>
    </message>
    <message>
        <location filename="../src/health/system_health.cpp" line="53"/>
        <source>Storages are full on the following Server:
%1.</source>
        <translation>以下服务器存储空间已满：
%1.</translation>
    </message>
    <message>
        <location filename="../src/health/system_health.cpp" line="55"/>
        <source>Storages are not configured on the following Server:
%1.</source>
        <translation>以下服务器存储空间未设置：
%1.</translation>
    </message>
    <message>
        <location filename="../src/health/system_health.cpp" line="59"/>
        <source>Rebuilding archive index is completed on the following Server:
%1.</source>
        <translation>将在以下服务器上完成录像文件索引重建：
%1.</translation>
    </message>
    <message>
        <location filename="../src/health/system_health.cpp" line="57"/>
        <source>You have no licenses.
You cannot record video from cameras.</source>
        <translation>无软件许可，
将不能对摄像机进行视频存储。</translation>
    </message>
</context>
<context>
    <name>QnTCPConnectionProcessor</name>
    <message>
        <location filename="../src/utils/network/tcp_connection_processor.cpp" line="238"/>
        <source>OK</source>
        <translation>确定</translation>
    </message>
    <message>
        <location filename="../src/utils/network/tcp_connection_processor.cpp" line="240"/>
        <source>Not Found</source>
        <translation>未找到</translation>
    </message>
    <message>
        <location filename="../src/utils/network/tcp_connection_processor.cpp" line="242"/>
        <source>Not Implemented</source>
        <translation>未执行</translation>
    </message>
    <message>
        <location filename="../src/utils/network/tcp_connection_processor.cpp" line="244"/>
        <source>Unsupported Transport</source>
        <translation>不支持的转换</translation>
    </message>
    <message>
        <location filename="../src/utils/network/tcp_connection_processor.cpp" line="246"/>
        <source>Internal Server Error</source>
        <translation>内部服务器错误</translation>
    </message>
    <message>
        <location filename="../src/utils/network/tcp_connection_processor.cpp" line="248"/>
        <source>Invalid Parameter</source>
        <translation>无效参数</translation>
    </message>
</context>
<context>
    <name>QnTranscoder</name>
    <message>
        <location filename="../src/transcoding/transcoder.cpp" line="272"/>
        <source>OpenCL transcoding is not implemented.</source>
        <translation>OpenCLT代码转换器未执行.</translation>
    </message>
    <message>
        <location filename="../src/transcoding/transcoder.cpp" line="275"/>
        <source>Unknown transcoding method.</source>
        <translation>未知编码转换方法.</translation>
    </message>
    <message>
        <location filename="../src/transcoding/transcoder.cpp" line="299"/>
        <source>OpenCLTranscode is not implemented</source>
        <translation>OpenCL代码转换器未执行</translation>
    </message>
    <message>
        <location filename="../src/transcoding/transcoder.cpp" line="302"/>
        <source>Unknown Transcode Method</source>
        <translation>未知编码转换方法</translation>
    </message>
</context>
<context>
    <name>QnTranslationListModel</name>
    <message>
        <location filename="../src/translation/translation_list_model.cpp" line="66"/>
        <source>%1 (built-in)</source>
        <translation>%1 (内建)</translation>
    </message>
    <message>
        <location filename="../src/translation/translation_list_model.cpp" line="68"/>
        <source>%1 (external)</source>
        <translation>%1 (外部)</translation>
    </message>
</context>
</TS>
