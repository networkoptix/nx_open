"use strict";angular.module("webadminApp",["ngCookies","ngResource","ngSanitize","ngRoute","ui.bootstrap","tc.chartjs"]).config(["$routeProvider",function(a){a.when("/settings",{templateUrl:"views/settings.html",controller:"SettingsCtrl"}).when("/join",{templateUrl:"views/join.html",controller:"SettingsCtrl"}).when("/info",{templateUrl:"views/info.html",controller:"InfoCtrl"}).when("/developers",{templateUrl:"views/developers.html",controller:"MainCtrl"}).when("/support",{templateUrl:"views/support.html",controller:"MainCtrl"}).when("/help",{templateUrl:"views/help.html",controller:"MainCtrl"}).when("/login",{templateUrl:"views/login.html",controller:"LoginCtrl"}).when("/advanced",{templateUrl:"views/advanced.html",controller:"AdvancedCtrl"}).when("/webclient",{templateUrl:"views/webclient.html",controller:"WebclientCtrl"}).when("/sdkeula",{templateUrl:"views/sdkeula.html",controller:"SdkeulaCtrl"}).otherwise({redirectTo:"/settings"})}]),angular.module("webadminApp").controller("MainCtrl",["$scope",function(a){a.awesomeThings=["HTML5 Boilerplate","AngularJS","Karma"]}]),angular.module("webadminApp").controller("NavigationCtrl",["$scope","$location","mediaserver",function(a,b,c){a.settings=c.getSettings(),a.settings.then(function(b){a.settings={name:b.data.reply.name,remoteAddresses:b.data.reply.remoteAddresses.join("\n")}}),a.isActive=function(a){var c=b.path().split("/")[1];return c.split("?")[0]===a.split("/")[1].split("?")[0]}}]),angular.module("webadminApp").directive("navbar",function(){return{restrict:"E",templateUrl:"views/navbar.html",controller:"NavigationCtrl"}}),angular.module("webadminApp").controller("SettingsCtrl",["$scope","$modal","$log","mediaserver","data",function(a,b,c,d,e){function f(){e.port=a.settings.port,b.open({templateUrl:"views/restart.html",controller:"RestartCtrl"})}function g(){return alert("Connection error"),!1}function h(a){if(0!=a.error){var b=a.data.errorString;switch(b){case"UNAUTHORIZED":case"password":b="Wrong password."}alert("Error: "+b)}else a.reply.restartNeeded?confirm("All changes saved. New settings will be applied after restart. \n Do you want to restart server now?")&&f():alert("Settings saved")}a.settings=d.getSettings(),a.settings.then(function(b){a.settings={systemName:b.data.reply.systemName,port:b.data.reply.port}}),a.password="",a.oldPassword="",a.confirmPassword="",a.openJoinDialog=function(){var e=b.open({templateUrl:"views/join.html",controller:"JoinCtrl",resolve:{items:function(){return a.settings}}});e.result.then(function(a){c.info(a),d.mergeSystems(a.url,a.password,a.keepMySystem).then(function(a){if(0!=a.data.error){var b=a.data.errorString;switch(b){case"FAIL":b="System is unreachable or doesn't exist.";break;case"UNAUTHORIZED":case"password":b="Wrong password.";break;case"INCOMPATIBLE":b="Found system has incompatible version.";break;case"url":b="Wrong url."}alert("Merge failed: "+b)}else alert("Merge succeed."),window.location.reload()})})},a.save=function(){d.saveSettings(a.settings.systemName,a.settings.port).success(h).error(g)},a.changePassword=function(){a.password==a.confirmPassword&&d.changePassword(a.password,a.oldPassword).success(h).error(g)},a.restart=function(){confirm("Do you want to restart server now?")&&f()}}]),angular.module("webadminApp").controller("JoinCtrl",["$scope","$modalInstance","$interval","mediaserver",function(a,b,c,d){a.settings={url:"",password:"",keepMySystem:!0},a.systems={joinSystemName:"NOT FOUND",systemName:"SYSTEMNAME",systemFound:!1},d.discoveredPeers().success(function(b){var c=_.map(b.reply,function(a){return{url:"http://"+a.remoteAddresses[0]+":"+a.port,systemName:a.systemName,ip:a.remoteAddresses[0],name:a.name}});_.filter(c,function(b){return b.systemName!=a.systems.systemName}),a.systems.discoveredUrls=c}),d.getSettings().success(function(b){a.systems.systemName=b.reply.systemName,a.systems.discoveredUrls=_.filter(a.systems.discoveredUrls,function(b){return b.systemName!=a.systems.systemName})}),a.test=function(){d.pingSystem(a.settings.url,a.settings.password).then(function(b){if(0!=b.data.error){var c=b.data.errorString;switch(c){case"FAIL":c="System is unreachable or doesn't exist.";break;case"UNAUTHORIZED":case"password":c="Wrong password.";break;case"INCOMPATIBLE":c="Found system has incompatible version.";break;case"url":c="Wrong url."}alert("Connection failed: "+c)}else a.systems.systemFound=!0,a.systems.joinSystemName=b.data.reply.systemName})},a.join=function(){b.close({url:a.settings.url,password:a.settings.password,keepMySystem:a.settings.keepMySystem})},a.selectSystem=function(b){a.settings.url=b.url},a.cancel=function(){b.dismiss("cancel")}}]),angular.module("webadminApp").factory("mediaserver",["$http","$resource",function(a){return{getSettings:function(){return a.get("/api/moduleInformation")},saveSettings:function(b,c){return a.post("/api/configure?systemName="+b+"&port="+c)},changePassword:function(b,c){return a.post("/api/configure?password="+b+"&oldPassword="+c)},mergeSystems:function(b,c,d){return a.post("/api/mergeSystems?password="+c+"&url="+encodeURIComponent(b)+"&takeRemoteSettings="+!d)},pingSystem:function(b,c){return a.post("/api/pingSystem?password="+c+"&url="+encodeURIComponent(b))},restart:function(){return a.post("/api/restart")},getStorages:function(){return a.get("/api/storageSpace")},discoveredPeers:function(){return a.get("/api/discoveredPeers")},getMediaServer:function(b){return a.get("/ec2/getMediaServers?id="+b)},saveMediaServer:function(b){return a.post("/ec2/saveMediaServer",b)},statistics:function(b){return b=b||"",a.get(b+"/api/statistics")}}}]),angular.module("webadminApp").directive("nxEqualEx",function(){return{require:"ngModel",link:function(a,b,c,d){return c.nxEqualEx?(a.$watch(c.nxEqualEx,function(a){void 0!==d.$viewValue&&""!==d.$viewValue&&(d.$setValidity("nxEqualEx",a===d.$viewValue),console.log("set valid to "+(a===d.$viewValue)))}),void d.$parsers.push(function(b){if(void 0===b||""===b)return d.$setValidity("nxEqualEx",!0),console.log("set valid to true"),b;var e=b===a.$eval(c.nxEqualEx);return d.$setValidity("nxEqualEx",e),console.log("set valid to "+e),e?b:void 0})):void console.error("nxEqualEx expects a model as an argument!")}}}),angular.module("webadminApp").controller("LoginCtrl",["$scope","auth",function(a,b){a.login=function(){a.loginForm.$valid?b.login(a.user):a.loginForm.submitted=!0}}]),angular.module("webadminApp").service("auth",["$http",function(a){this.login=function(b){return a.post("/ec2/login",{username:b.username,password:b.password})}}]),angular.module("webadminApp").directive("icon",function(){return{restrict:"E",scope:{"for":"="},template:'<span ng-show="for || onFalse" class="glyphicon glyphicon-{{for?onTrue:onFalse}} {{class}}" title="{{for?titleTrue:titleFalse}}"></span>',link:function(a,b,c){a.onTrue=c.onTrue||"ok",a.onFalse=c.onFalse,a.class=c.class,a.titleTrue=c.titleTrue,a.titleFalse=c.titleFalse}}}),angular.module("webadminApp").directive("inplaceEdit",function(){return{restrict:"E",templateUrl:"views/inplace-edit.html",scope:{model:"=ngModel"},link:function(a,b,c){var d=a.model;a.type=c.type||"text",a.placeholder=c.placeholder,a.inputId=c.inputId,a.edit=function(){d=a.model,b.find("input").val(d),b.find(".read-container").hide(),b.find(".edit-container").show(),b.find("input").focus()},a.save=function(){d!=a.model&&(console.log("we should call save function here, val:",c.nxOnsave),a.$parent.$eval(c.nxOnsave)),d=a.model,b.find(".read-container").show(),b.find(".edit-container").hide()},a.cancel=function(){a.model=d,a.$apply(),console.log("cancel",d),b.find(".read-container").show(),b.find(".edit-container").hide()},b.find(".edit-container").hide()}}}),angular.module("webadminApp").controller("AdvancedCtrl",["$scope","$modal","$log","mediaserver",function(a,b,c,d){function e(a){return decodeURIComponent(a.replace(/file:\/\/.+?:.+?\//gi,""))}a.storages=d.getStorages(),a.storages.then(function(b){a.storages=_.sortBy(b.data.reply.storages,function(a){return e(a.url)});for(var c in a.storages)a.storages[c].reservedSpaceGb=Math.round(1*a.storages[c].reservedSpace/1073741824),a.storages[c].url=e(a.storages[c].url);a.$watch(function(){for(var b in a.storages){var c=a.storages[b];c.reservedSpace=1073741824*c.reservedSpaceGb,c.warning=c.isUsedForWriting&&(c.reservedSpace<=0||c.reservedSpace>=c.totalSpace)}})}),a.formatSpace=function(a){var b=2,c=["Bytes","KB","MB","GB","TB"],d=0;if(0==a)return"n/a";for(;a>=1024;)d++,a/=1024;return Number(a).toFixed(b)+" "+c[d]},a.toGBs=function(a){return Math.floor(a/1073741824)},a.save=function(){var b,c=!1;_.each(a.storages,function(a){b=b||a.isUsedForWriting,a.reservedSpace>a.freeSpace&&(c="Set reserved space is greater than free space left. Possible partial remove of the video footage is expected. Do you want to continue?")}),b||(c="No storages were selected for writing - video will not be recorded on this server. Do you want to continue?"),(!c||confirm(c))&&d.getSettings().then(function(b){d.getMediaServer(b.data.reply.id.replace("{","").replace("}","")).then(function(b){var c=b.data[0];_.each(a.storages,function(a){var b=_.findWhere(c.storages,{id:a.storageId});null!=b&&(b.spaceLimit=a.reservedSpace,b.usedForWriting=a.isUsedForWriting)}),d.saveMediaServer(c).error(function(){alert("Error: Couldn't save settings")}).then(function(){alert("Settings saved")})})})},a.cancel=function(){window.location.reload()}}]),angular.module("webadminApp").controller("InfoCtrl",["$scope","mediaserver",function(a,b){function c(a){return decodeURIComponent(a.replace(/file:\/\/.+?:.+?\//gi,""))}a.storages=b.getStorages(),a.storages.then(function(b){a.storages=_.sortBy(b.data.reply.storages,function(a){return c(a.url)}),_.each(a.storages,function(a){a.url=c(a.url)})}),a.formatSpace=function(a){var b=2,c=["Bytes","KB","MB","GB","TB"],d=0;if(0==a)return"n/a";for(;a>=1024;)d++,a/=1024;return Number(a).toFixed(b)+" "+c[d]}}]),angular.module("webadminApp").controller("RestartCtrl",["$scope","$modalInstance","$interval","mediaserver","data",function(a,b,c,d,e){function f(){return window.location.href=a.url,window.location.reload(a.url),!1}function g(){d.statistics(h).success(function(b){var c=b.reply.uptimeMs;return i>c||j?(a.state="server is starting",f()):(a.state="server is restarting",i=c,void setTimeout(g,1e3))}).error(function(){return a.state="server is offline",j=!0,setTimeout(g,1e3),!1})}a.url=window.location.protocol+"//"+window.location.hostname+":"+e.port+window.location.pathname+window.location.search,a.state="";var h=window.location.protocol+"//"+window.location.hostname+":"+e.port,i=Number.MAX_VALUE,j=!1;a.refresh=f,d.statistics().success(function(a){i=a.reply.uptimeMs,d.restart().then(function(){g()})}).error(function(){return a.state="server is offline",setTimeout(g,1e3),!1})}]),angular.module("webadminApp").controller("SdkeulaCtrl",["$scope",function(a){a.agree=!1,a.next=function(){return window.open("sdk.zip"),!1}}]),angular.module("webadminApp").controller("WebclientCtrl",["$scope","$modalInstance","$interval","mediaserver",function(){}]),angular.module("webadminApp").factory("data",function(){return{port:0}}),angular.module("webadminApp").controller("HealthCtrl",["$scope","$modal","$log","mediaserver",function(a,b,c,d){function e(b){for(var c=[{label:"",fillColor:i,strokeColor:i,pointColor:i,pointStrokeColor:i,pointHighlightFill:i,pointHighlightStroke:i,show:!0,data:Array.apply(null,new Array(a.healthLength)).map(Number.prototype.valueOf,100)}],d=0;d<b.length;d++){var e=h[b[d].deviceType],f=e[d%e.length];c.push({label:b[d].description,fillColor:f,strokeColor:f,pointColor:f,pointStrokeColor:f,pointHighlightFill:f,pointHighlightStroke:f,show:!0,data:Array.apply(null,new Array(a.healthLength)).map(Number.prototype.valueOf,0)})}a.datasets=c,a.updateVisibleDatasets()}function f(b){for(var c=a.datasets,d=0;d<c.length;d++){var e=c[d],f=0,g=_.filter(b,function(a){return a.description==e.label});g&&g.length>0&&(f=g[0].value),f>100&&console.log(f,e,g),0!=d&&(e.data.push(100*f),e.data.length>a.healthLength&&(e.data=e.data.slice(e.data.length-a.healthLength,e.data.length)))}}function g(){d.statistics().then(function(b){return 0==j&&e(b.data.reply.statistics),f(200==b.status&&0==b.data.error?b.data.reply.statistics:[]),j=setTimeout(g,a.interval),!1})}a.healthLength=100,a.interval=1e3,a.data={labels:Array.apply(null,new Array(a.healthLength)).map(String.prototype.valueOf,""),datasets:[{label:"",fillColor:i,strokeColor:i,pointColor:i,pointStrokeColor:i,pointHighlightFill:i,pointHighlightStroke:i,show:!0,data:Array.apply(null,new Array(a.healthLength)).map(Number.prototype.valueOf,100)}]},a.legend="",a.options={animation:!1,scaleOverride:!1,scaleSteps:4,scaleShowLabels:!1,scaleLabel:"<%=value%> %",scaleIntegersOnly:!0,responsive:!0,scaleShowGridLines:!0,scaleGridLineColor:"rgba(0,0,0,.05)",scaleGridLineWidth:1,bezierCurve:!0,bezierCurveTension:.4,pointDot:!1,pointDotRadius:0,pointDotStrokeWidth:1,pointHitDetectionRadius:0,datasetStroke:!0,datasetStrokeWidth:2,datasetFill:!1,onAnimationProgress:function(){},onAnimationComplete:function(){},legendTemplate:'<ul class="tc-chart-js-legend"><% for (var i=0; i<datasets.length; i++){%><li><span style="background-color:<%=datasets[i].strokeColor%>"></span><%if(datasets[i].label){%><%=datasets[i].label%><%}%></li><%}%></ul>'};var h={StatisticsCPU:["#3776ca"],StatisticsRAM:["#db3ba9"],StatisticsHDD:["#438231","#484848","#aa880b","#b25e26","#9200d1","#00d5d5","#267f00","#8f5656","#c90000"],StatisticsNETWORK:["#ff3434","#b08f4c","#8484ff","#34ff84"]},i="rgba(255,255,255,0)";a.updateVisibleDatasets=function(){a.data.datasets=_.filter(a.datasets,function(a){return a.show})};var j=0;g()}]);