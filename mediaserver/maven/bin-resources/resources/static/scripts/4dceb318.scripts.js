"use strict";var Config={globalEditServersPermissions:32,helpLinks:[]};angular.module("webadminApp",["ipCookie","ngResource","ngSanitize","ngRoute","ui.bootstrap","tc.chartjs","com.2fdevs.videogular","com.2fdevs.videogular.plugins.controls","com.2fdevs.videogular.plugins.buffering","com.2fdevs.videogular.plugins.dash","com.2fdevs.videogular.plugins.poster"]).config(["$routeProvider",function(a){a.when("/settings",{templateUrl:"views/settings.html",controller:"SettingsCtrl"}).when("/join",{templateUrl:"views/join.html",controller:"SettingsCtrl"}).when("/info",{templateUrl:"views/info.html",controller:"InfoCtrl"}).when("/developers",{templateUrl:"views/developers.html",controller:"MainCtrl"}).when("/support",{templateUrl:"views/support.html",controller:"MainCtrl"}).when("/help",{templateUrl:"views/help.html",controller:"MainCtrl"}).when("/login",{templateUrl:"views/login.html"}).when("/advanced",{templateUrl:"views/advanced.html",controller:"AdvancedCtrl"}).when("/webclient",{templateUrl:"views/webclient.html",controller:"WebclientCtrl",reloadOnSearch:!1}).when("/view/",{templateUrl:"views/view.html",controller:"ViewCtrl",reloadOnSearch:!1}).when("/view/:cameraId",{templateUrl:"views/view.html",controller:"ViewCtrl",reloadOnSearch:!1}).when("/sdkeula",{templateUrl:"views/sdkeula.html",controller:"SdkeulaCtrl"}).otherwise({redirectTo:"/settings"})}]),angular.module("webadminApp").run(["$route","$rootScope","$location",function(a,b,c){var d=c.path;c.path=function(e,f){if(f===!1)var g=a.current,h=b.$on("$locationChangeSuccess",function(){a.current=g,h()});return d.apply(c,[e])}}]),angular.module("webadminApp").factory("mediaserver",["$http","$modal","ipCookie","$log",function(a,b,c,d){function e(){return a.get("/api/moduleInformation?salt="+(new Date).getTime())}function f(a){return 401===a.status?void(null===k&&(k=b.open({templateUrl:"views/login.html",keyboard:!1,backdrop:"static"}),k.result.then(function(){k=null}))):(h=null,void(null===j&&e(!0).catch(function(a){d.error(a),j=b.open({templateUrl:"offline_modal",controller:"OfflineCtrl",keyboard:!1,backdrop:"static"}),j.result.then(function(){j=null})})))}function g(a){return a.catch(f),a}var h=null,i=null;c("Authorization","Digest",{path:"/"});var j=null,k=null;return{getSettings:function(b){return b=b||"",b===!0?(h=null,e()):""===b?(null===h&&(h=g(e())),h):a.get(b+"/api/moduleInformation",{timeout:3e3})},saveSettings:function(b,c){return g(a.post("/api/configure?systemName="+b+"&port="+c))},changePassword:function(b,c){return g(a.post("/api/configure?password="+b+"&oldPassword="+c))},mergeSystems:function(b,c,d,e){return g(a.post("/api/mergeSystems?password="+c+"&currentPassword="+d+"&url="+encodeURIComponent(b)+"&takeRemoteSettings="+!e))},pingSystem:function(b,c){return g(a.post("/api/pingSystem?password="+c+"&url="+encodeURIComponent(b)))},restart:function(){return g(a.post("/api/restart"))},getStorages:function(){return g(a.get("/api/storageSpace"))},saveStorages:function(b){return g(a.post("/ec2/saveStorages",b))},discoveredPeers:function(){return g(a.get("/api/discoveredPeers"))},getMediaServer:function(b){return g(a.get("/ec2/getMediaServersEx?id="+b.replace("{","").replace("}","")))},getMediaServers:function(){return g(a.get("/ec2/getMediaServersEx"))},getCameras:function(b){return g("undefined"!=typeof b?a.get("/ec2/getCamerasEx?id="+b.replace("{","").replace("}","")):a.get("/ec2/getCamerasEx"))},saveMediaServer:function(b){return g(a.post("/ec2/saveMediaServer",b))},statistics:function(b){return b=b||"",g(a.get(b+"/api/statistics?salt="+(new Date).getTime()))},getCurrentUser:function(b){return(null===i||b)&&(i=g(a.get("/api/getCurrentUser"))),i},getRecords:function(b,c,d,e,f,h){var i=new Date;return"undefined"==typeof d&&(d=i.getTime()-2592e6),"undefined"==typeof e&&(e=i.getTime()+1e5),"undefined"==typeof f&&(f=(e-d)/1e3),"undefined"==typeof h&&(h=0),"/"!==b&&""!==b&&null!==b&&(b="/proxy/"+b+"/"),g(a.get(b+"api/RecordedTimePeriods?physicalId="+c+"&startTime="+d+"&endTime="+e+"&detail="+f+"&periodsType="+h))}}}]),angular.module("webadminApp").service("auth",["$http",function(a){this.login=function(b){return a.post("/ec2/login",{username:b.username,password:b.password})}}]),angular.module("webadminApp").directive("navbar",function(){return{restrict:"E",templateUrl:"views/navbar.html",controller:"NavigationCtrl",scope:{noPanel:"="}}}),angular.module("webadminApp").directive("nxEqualEx",["$log",function(a){return{require:"ngModel",link:function(b,c,d,e){function f(){console.log("updateValidity",e.$viewValue,b.$eval(d.nxEqualEx)),void 0!==e.$viewValue&&""!==e.$viewValue&&e.$setValidity("nxEqualEx",b.$eval(d.nxEqualEx)===e.$viewValue)}return d.nxEqualEx?(b.$watch(d.nxEqualEx,f),b.$watch("$viewValue",f),void e.$parsers.push(function(a){if(void 0===a||""===a)return e.$setValidity("nxEqualEx",!0),a;var c=a===b.$eval(d.nxEqualEx);return e.$setValidity("nxEqualEx",c),a})):void a.error("nxEqualEx expects a model as an argument!")}}}]),angular.module("webadminApp").directive("icon",function(){return{restrict:"E",scope:{"for":"="},template:"<span ng-if='for || onFalse' class='glyphicon glyphicon-{{for?onTrue:onFalse}} {{class}}' title='{{for?titleTrue:titleFalse}}'></span>",link:function(a,b,c){a.onTrue=c.onTrue||"ok",a.onFalse=c.onFalse,a["class"]=c["class"],a.titleTrue=c.titleTrue,a.titleFalse=c.titleFalse}}}),angular.module("webadminApp").directive("fileupload",function(){return{restrict:"E",template:"<span class='btn btn-success fileinput-button' ><span>{{text}}</span><input type='file' name='installerFile1' class='fileupload' id='fileupload' ></span><div class='progress' style='display:none;'><div class='progress-bar progress-bar-success progress-bar-striped'></div></div>",link:function(a,b,c){b.find(".fileupload").fileupload({url:c.url,dataType:"json",done:function(a,c){if("0"===c.result.error)alert("Updating successfully started. It will take several minutes");else switch(c.result.errorString){case"UP_TO_DATE":alert("Updating failed. The provided version is already installed.");break;case"INVALID_FILE":alert("Updating failed. Provided file is not a valid update archive.");break;case"INCOMPATIBLE_SYSTEM":alert("Updating failed. Provided file is targeted for another system.");break;case"EXTRACTION_ERROR":alert("Updating failed. Extraction failed, check available storage.");break;case"INSTALLATION_ERROR":alert("Updating failed. Couldn't execute installation script.")}b.find(".progress").hide()},progressall:function(a,c){var d=parseInt(c.loaded/c.total*100,10);b.find(".progress").show(),b.find(".progress-bar").css("width",d+"%")}}).prop("disabled",!$.support.fileInput).parent().addClass($.support.fileInput?void 0:"disabled"),a.text=c.text||"Select files"}}}),angular.module("com.2fdevs.videogular.plugins.controls").directive("vgQuality",function(){return{restrict:"E",require:"^videogular",transclude:!0,template:'<button class="iconButton enter" ng-click="onClickQuality()" ng-class="qualityIcon" aria-label="Chose quality"></button>',scope:{current:"=",list:"="},link:function(){}}}),angular.module("webadminApp").directive("inplaceEdit",function(){return{restrict:"E",templateUrl:"views/inplace-edit.html",scope:{model:"=ngModel"},link:function(a,b,c){var d=a.model;a.type=c.type||"text",a.placeholder=c.placeholder,a.inputId=c.inputId,a.edit=function(){d=a.model,b.find("input").val(d),b.find(".read-container").hide(),b.find(".edit-container").show(),b.find("input").focus()},a.save=function(){d!==a.model&&a.$parent.$eval(c.nxOnsave),d=a.model,b.find(".read-container").show(),b.find(".edit-container").hide()},a.cancel=function(){a.model=d,a.$apply(),b.find(".read-container").show(),b.find(".edit-container").hide()},b.find(".edit-container").hide()}}}),angular.module("webadminApp").controller("MainCtrl",["$scope",function(a){a.config=Config}]),angular.module("webadminApp").controller("NavigationCtrl",["$scope","$location","mediaserver","ipCookie",function(a,b,c,d){a.user={isAdmin:!0},c.getCurrentUser().then(function(b){a.user={isAdmin:b.data.reply.isAdmin||b.data.reply.permissions&Config.globalEditServersPermissions}}),c.getSettings().then(function(b){a.settings=b.data.reply,a.settings.remoteAddresses=a.settings.remoteAddresses.join("\n")}),a.isActive=function(a){var c=b.path().split("/")[1];return c.split("?")[0]===a.split("/")[1].split("?")[0]},a.logout=function(){d.remove("response",{path:"/"}),d.remove("nonce",{path:"/"}),d.remove("realm",{path:"/"}),d.remove("username",{path:"/"}),window.location.reload()}}]),angular.module("webadminApp").controller("SettingsCtrl",["$scope","$modal","$log","mediaserver","$location","$timeout",function(a,b,c,d,e,f){function g(c){b.open({templateUrl:"views/restart.html",controller:"RestartCtrl",resolve:{port:function(){return c?a.settings.port:null}}})}function h(){return alert("Connection error"),!1}function i(b){var c=b.data;if("0"!==c.error){var d=c.errorString;switch(d){case"UNAUTHORIZED":case"password":case"PASSWORD":d="Wrong password."}alert("Error: "+d)}else c.reply.restartNeeded?confirm("All changes saved. New settings will be applied after restart. \n Do you want to restart server now?")&&g(!0):(alert("Settings saved"),a.settings.port!==window.location.port?window.location.href=window.location.protocol+"//"+window.location.hostname+":"+a.settings.port:window.location.reload())}function j(b,c){var e=b.networkAddresses.split(";"),f=b.apiUrl.substring(b.apiUrl.lastIndexOf(":")),g="http://"+e[c]+f;d.getSettings(g).then(function(){b.apiUrl=g},function(){return c<e.length-1?j(b,c+1):(b.status="Unavailable",a.mediaServers=_.sortBy(a.mediaServers,function(a){return("Online"===a.status?"0":"1")+a.Name+a.id})),!1})}d.getCurrentUser().then(function(a){return a.data.reply.isAdmin||a.data.reply.permissions&Config.globalEditServersPermissions?void 0:void e.path("/info")}),d.getSettings().then(function(b){a.settings={systemName:b.data.reply.systemName,port:b.data.reply.port,id:b.data.reply.id},a.oldSystemName=b.data.reply.systemName,a.oldPort=b.data.reply.port}),a.password="",a.oldPassword="",a.confirmPassword="",a.openJoinDialog=function(){var e=b.open({templateUrl:"views/join.html",controller:"JoinCtrl",resolve:{items:function(){return a.settings}}});e.result.then(function(a){c.info(a),d.mergeSystems(a.url,a.password,a.currentPassword,a.keepMySystem).then(function(a){if("0"!==a.data.error){var b=a.data.errorString;switch(b){case"FAIL":b="System is unreachable or doesn't exist.";break;case"UNAUTHORIZED":case"password":case"PASSWORD":b="Wrong password.";break;case"INCOMPATIBLE":b="Found system has incompatible version.";break;case"url":case"URL":b="Wrong url."}alert("Merge failed: "+b)}else alert("Merge succeed."),window.location.reload()})})},a.save=function(){a.settingsForm.$valid?(a.oldSystemName===a.settings.systemName||confirm('If there are others servers in local network with "'+a.settings.systemName+'" system name then it could lead to this server settings loss. Continue?')||(a.settings.systemName=a.oldSystemName),(a.oldSystemName!==a.settings.systemName||a.oldPort!==a.settings.port)&&d.saveSettings(a.settings.systemName,a.settings.port).then(i,h)):alert("form is not valid")},a.changePassword=function(){a.password===a.confirmPassword&&d.changePassword(a.password,a.oldPassword).then(i,h)},a.restart=function(){confirm("Do you want to restart server now?")&&g(!1)},d.getMediaServers().then(function(b){a.mediaServers=_.sortBy(b.data,function(a){return("Online"===a.status?"0":"1")+a.Name+a.id}),f(function(){_.each(a.mediaServers,function(a){var b=0;j(a,b)})},1e3)})}]),angular.module("webadminApp").controller("JoinCtrl",["$scope","$modalInstance","$interval","mediaserver",function(a,b,c,d){a.settings={url:"",password:"",currentPassword:"",keepMySystem:!0},a.systems={joinSystemName:"NOT FOUND",systemName:"SYSTEMNAME",systemFound:!1},d.discoveredPeers().then(function(b){var c=_.map(b.data.reply,function(a){return{url:"http://"+a.remoteAddresses[0]+":"+a.port,systemName:a.systemName,ip:a.remoteAddresses[0],name:a.name}});_.filter(c,function(b){return b.systemName!==a.systems.systemName}),a.systems.discoveredUrls=c}),d.getSettings().then(function(b){a.systems.systemName=b.data.reply.systemName,a.systems.discoveredUrls=_.filter(a.systems.discoveredUrls,function(b){return b.systemName!==a.systems.systemName})}),a.test=function(){d.pingSystem(a.settings.url,a.settings.password).then(function(b){if("0"!==b.data.error){var c=b.data.errorString;switch(c){case"FAIL":c="System is unreachable or doesn't exist.";break;case"UNAUTHORIZED":case"password":c="Wrong password.";break;case"INCOMPATIBLE":c="Found system has incompatible version.";break;case"url":c="Wrong url."}alert("Connection failed: "+c)}else a.systems.systemFound=!0,a.systems.joinSystemName=b.data.reply.systemName})},a.join=function(){b.close({url:a.settings.url,password:a.settings.password,keepMySystem:a.settings.keepMySystem,currentPassword:a.settings.currentPassword})},a.selectSystem=function(b){a.settings.url=b.url},a.cancel=function(){b.dismiss("cancel")}}]),angular.module("webadminApp").controller("OfflineCtrl",["$scope","$modalInstance","$interval","mediaserver",function(a,b,c,d){function e(){return window.location.reload(),!1}function f(){var a=d.getSettings(!0);a.then(function(){return e()},function(){return setTimeout(f,1e3),!1})}a.refresh=e,a.state="server is offline",setTimeout(f,1e3)}]),angular.module("webadminApp").controller("LoginCtrl",["$scope","mediaserver","ipCookie",function(a,b,c){function d(){return window.location.reload(),!1}a.login=function(){if(console.log(a),a.loginForm.$valid){var e=c("realm"),f=c("nonce"),g=md5(a.user.username+":"+e+":"+a.user.password),h=md5("GET:"),i=md5(g+":"+f+":"+h);c("response",i,{path:"/"}),c("username",a.user.username,{path:"/"}),b.getCurrentUser(!0).then(d).catch(function(){alert("not authorized")})}}}]),angular.module("webadminApp").controller("AdvancedCtrl",["$scope","$modal","$log","mediaserver","$location",function(a,b,c,d,e){function f(a){return decodeURIComponent(a.replace(/file:\/\/.+?:.+?\//gi,""))}d.getCurrentUser().then(function(a){a.data.reply.isAdmin||a.data.reply.permissions&Config.globalEditServersPermissions||e.path("/info")}),a.someSelected=!1,a.reduceArchiveWarning=!1,d.getStorages().then(function(b){a.storages=_.sortBy(b.data.reply.storages,function(a){return f(a.url)});for(var c=0;c<a.storages.length;c++)a.storages[c].reservedSpaceGb=Math.round(a.storages[c].reservedSpace/1073741824),a.storages[c].url=f(a.storages[c].url);a.$watch(function(){a.reduceArchiveWarning=!1,a.someSelected=!1;for(var b in a.storages){var c=a.storages[b];c.reservedSpace=1073741824*c.reservedSpaceGb,a.someSelected=a.someSelected||c.isUsedForWriting&&c.isWritable,c.reservedSpace>c.freeSpace&&(a.reduceArchiveWarning=!0)}})}),a.formatSpace=function(a){var b=2,c=["Bytes","KB","MB","GB","TB"],d=0;if(0===a)return"n/a";for(;a>=1024;)d++,a/=1024;return Number(a).toFixed(b)+" "+c[d]},a.toGBs=function(a){return Math.floor(a/1073741824)},a.update=function(){},a.save=function(){var b,c=!1;_.each(a.storages,function(a){b=b||a.isUsedForWriting,a.reservedSpace>a.freeSpace&&(c="Set reserved space is greater than free space left. Possible partial remove of the video footage is expected. Do you want to continue?")}),b||(c="No storages were selected for writing - video will not be recorded on this server. Do you want to continue?"),(!c||confirm(c))&&d.getSettings().then(function(b){d.getMediaServer(b.data.reply.id.replace("{","").replace("}","")).then(function(b){var c=b.data[0];_.each(a.storages,function(a){var b=_.findWhere(c.storages,{id:a.storageId});null!==b&&(b.spaceLimit=a.reservedSpace,b.usedForWriting=a.isUsedForWriting)}),d.saveStorages(c.storages).then(function(a){if("undefined"!=typeof a.error&&"0"!==a.error){var b=a.errorString;alert("Error: "+b)}else alert("Settings saved")},function(){alert("Error: Couldn't save settings")})})})},a.cancel=function(){window.location.reload()}}]),angular.module("webadminApp").controller("InfoCtrl",["$scope","mediaserver",function(a,b){function c(a){return decodeURIComponent(a.replace(/file:\/\/.+?:.+?\//gi,""))}b.getSettings().then(function(b){a.settings=b.data.reply}),b.getStorages().then(function(b){a.storages=_.sortBy(b.data.reply.storages,function(a){return c(a.url)}),_.each(a.storages,function(a){a.url=c(a.url)})}),a.formatSpace=function(a){var b=2,c=["Bytes","KB","MB","GB","TB"],d=0;if(0===a)return"n/a";for(;a>=1024;)d++,a/=1024;return Number(a).toFixed(b)+" "+c[d]}}]),angular.module("webadminApp").controller("RestartCtrl",["$scope","$modalInstance","$interval","mediaserver","port",function(a,b,c,d,e){function f(){return window.location.href=h,!1}function g(){var b=j?d.getSettings(h):d.statistics(h);b.then(function(b){return j||b.data.reply.uptimeMs<i?(a.state="server is starting",f()):(a.state="server is restarting",i=b.data.reply.uptimeMs,void setTimeout(g,1e3))},function(){return a.state="server is offline",j=!0,setTimeout(g,1e3),!1})}e=e||window.location.port,a.state="";var h=window.location.protocol+"//"+window.location.hostname+":"+e;a.url=h+window.location.pathname+window.location.search;var i=Number.MAX_VALUE,j=!1;a.refresh=f,d.statistics().then(function(a){i=a.data.reply.uptimeMs,d.restart().then(function(){g()},function(){return g(),!1})},function(){return a.state="server is offline",setTimeout(g,1e3),!1})}]),angular.module("webadminApp").controller("SdkeulaCtrl",["$scope",function(a){a.agree=!1,a.next=function(){return window.open("nx_sdk-archive.zip"),!1}}]),angular.module("webadminApp").controller("WebclientCtrl",function(){}),angular.module("webadminApp").controller("ViewCtrl",["$scope","$rootScope","$location","$routeParams","mediaserver",function(a,b,c,d,e){function f(b){return _.find(a.mediaServers,function(a){return a.id===b.parentId})}function g(b){return a.settings.id===b.id.replace("{","").replace("}","")?"":b.apiUrl.replace("http://","").replace("https://","")}function h(){var b=a.activeCamera.physicalId,c=f(a.activeCamera),d="";a.settings.id!==c.id.replace("{","").replace("}","")&&(d="/proxy/"+g(c)),a.acitveVideoSource=[{src:d+"/media/"+b+".webm?resolution="+a.activeResolution,type:"video/webm"},{src:d+"/media/"+b+".mp4?resolution="+a.activeResolution,type:"video/mp4"},{src:d+"/hls/"+b+".m3u?resolution="+a.activeResolution},{src:d+"/hls/"+b+".m3u8?resolution="+a.activeResolution},{src:d+"/media/"+b+".mpegts?resolution="+a.activeResolution},{src:d+"/media/"+b+".3gp?resolution="+a.activeResolution},{src:d+"/media/"+b+".mpjpeg?resolution="+a.activeResolution}]}function i(a){return a=a.split("/")[2]||a.split("/")[0],a.split(":")[0].split("?")[0]}function j(){e.getCameras().then(function(b){function c(a){a.url=i(a.url);var b=0;try{for(var c=a.url.split("."),d=0;d<c.length;d++){var e=3-d;b+=parseInt(c[d])%256*Math.pow(256,e)}if(isNaN(b))throw b;b=b.toString(16),b.length<8&&(b="0"+b)}catch(f){b=a.url}return a.name+"__"+b}var d=b.data,e=_.partition(d,function(a){return""===a.groupId});e[1]=_.sortBy(e[1],c),e[1]=_.groupBy(e[1],function(a){return a.groupId}),e[1]=_.values(e[1]),e[1]=_.map(e[1],function(a){return{isGroup:!0,collapsed:!1,parentId:a[0].parentId,name:a[0].groupName,id:a[0].groupId,url:a[0].url,status:"Online",cameras:a}}),d=_.union(e[0],e[1]),d=_.sortBy(d,c),a.cameras=_.groupBy(d,function(a){return a.parentId}),a.allcameras=d},function(){alert("network problem")})}a.cameras={},a.activeCamera=null,a.activeResolution="320p",a.availableResolutions=["1080p","720p","640p","320p","240p"],a.settings={id:null},e.getSettings().then(function(b){a.settings={id:b.data.reply.id}}),a.selectCameraById=function(b){a.cameraId=b||a.cameraId,a.activeCamera=_.find(a.allcameras,function(b){return b.id===a.cameraId}),a.activeCamera&&h()},b.$on("$routeChangeStart",function(b,c){a.selectCameraById(c.params.cameraId)}),a.selectCamera=function(b){c.path("/view/"+b.id,!1),a.selectCameraById(b.id)},a.$watch("allcameras",function(){a.selectCameraById()}),e.getMediaServers().then(function(b){_.each(b.data,function(a){a.url=i(a.url),a.collapsed="Online"!==a.status&&(a.allowAutoRedundancy||a.flags.indexOf("SF_Edge")<0)}),a.mediaServers=b.data,j(),a.selectCameraById(d.cameraId)},function(){alert("network problem")})}]),angular.module("webadminApp").controller("HealthCtrl",["$scope","$modal","$log","mediaserver","$timeout",function(a,b,c,d,e){function f(b){for(var c=[{label:"",fillColor:j,strokeColor:j,pointColor:j,pointStrokeColor:j,pointHighlightFill:j,pointHighlightStroke:j,show:!0,hideLegend:!0,data:Array.apply(null,new Array(a.healthLength)).map(Number.prototype.valueOf,100)},{label:"",fillColor:j,strokeColor:j,pointColor:j,pointStrokeColor:j,pointHighlightFill:j,pointHighlightStroke:j,show:!0,hideLegend:!0,data:Array.apply(null,new Array(a.healthLength)).map(Number.prototype.valueOf,0)}],d=0;d<b.length;d++){var e=i[b[d].deviceType],f=e[d%e.length];c.push({label:b[d].description,fillColor:f,strokeColor:f,pointColor:f,pointStrokeColor:f,pointHighlightFill:f,pointHighlightStroke:f,show:!0,hideLegend:!1,data:Array.apply(null,new Array(a.healthLength)).map(Number.prototype.valueOf,0)})}a.datasets=c,a.updateVisibleDatasets()}function g(b){for(var c=a.datasets,d=function(a){return a.description===f.label},e=2;e<c.length;e++){var f=c[e],g=0,h=_.filter(b,d);h&&h.length>0&&(g=h[0].value),f.data.push(100*g),f.data.length>a.healthLength&&(f.data=f.data.slice(f.data.length-a.healthLength,f.data.length))}}function h(){d.statistics().then(function(b){return null===k&&f(b.data.reply.statistics),a.serverIsOnline=!0,g(200===b.status&&"0"===b.data.error?b.data.reply.statistics:[]),k=e(h,a.interval),!1},function(){g([]),a.serverIsOnline=!1,k=e(h,a.interval)})}a.healthLength=100,a.interval=1e3,a.serverIsOnline=!0;var i={StatisticsCPU:["#3776ca"],StatisticsRAM:["#db3ba9"],StatisticsHDD:["#438231","#484848","#aa880b","#b25e26","#9200d1","#00d5d5","#267f00","#8f5656","#c90000"],StatisticsNETWORK:["#ff3434","#b08f4c","#8484ff","#34ff84"]},j="rgba(255,255,255,0)";a.data={labels:Array.apply(null,new Array(a.healthLength)).map(String.prototype.valueOf,""),datasets:[{label:"",fillColor:j,strokeColor:j,pointColor:j,pointStrokeColor:j,pointHighlightFill:j,pointHighlightStroke:j,show:!0,hideLegend:!0,data:Array.apply(null,new Array(a.healthLength)).map(Number.prototype.valueOf,100)}]},a.legend="",a.options={animation:!1,scaleOverride:!1,scaleSteps:10,scaleShowLabels:!1,scaleLabel:"<%=value%> %",scaleIntegersOnly:!0,responsive:!0,scaleShowGridLines:!0,scaleGridLineColor:"rgba(0,0,0,.05)",scaleGridLineWidth:1,bezierCurve:!0,bezierCurveTension:.4,pointDot:!1,pointDotRadius:0,pointDotStrokeWidth:1,pointHitDetectionRadius:0,datasetStroke:!0,datasetStrokeWidth:2,datasetFill:!1,onAnimationProgress:function(){},onAnimationComplete:function(){},legendTemplate:'<ul class="tc-chart-js-legend"><% for (var i=0; i<datasets.length; i++){%><li><span style="background-color:<%=datasets[i].strokeColor%>"></span><%if(datasets[i].label){%><%=datasets[i].label%><%}%></li><%}%></ul>'},a.updateVisibleDatasets=function(){a.data.datasets=_.filter(a.datasets,function(a){return a.show})};var k=null,l=setTimeout(h,a.interval);a.$on("$destroy",function(){e.cancel(k),clearTimeout(l)})}]);