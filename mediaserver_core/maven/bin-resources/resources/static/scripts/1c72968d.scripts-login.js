"use strict";var Config={globalEditServersPermissions:32,globalViewArchivePermission:256,globalViewLivePermission:128,demo:"/~ebalashov/webclient/api",demoMedia:"//10.0.2.186:7001",webclientEnabled:!0,allowDebugMode:!1,debug:{video:!0,videoFormat:!1,chunksOnTimeline:!1},helpLinks:[]};angular.module("webadminApp",["ipCookie","ngResource","ngSanitize","ngRoute","ui.bootstrap","ngStorage"]),angular.module("webadminApp").factory("mediaserver",["$http","$modal","$q","ipCookie","$log",function(a,b,c,d,e){function f(){return a.get(m+"/api/moduleInformation?showAddresses=true")}function g(a){if(401===a.status){var c=window.self!==window.top;return void(c||null!==r||(r=b.open({templateUrl:"views/login.html",keyboard:!1,backdrop:"static"}),r.result.then(function(){r=null})))}0!==a.status&&(console.log(a),k=null,null===q&&f(!0)["catch"](function(a){q=b.open({templateUrl:"offline_modal",controller:"OfflineCtrl",keyboard:!1,backdrop:"static"}),q.result.then(function(a){q=null})}))}function h(a){return a["catch"](g),a}function i(b,c){return h(a.post(b,c))}function j(b){var d=c.defer(),e=h(a.get(b,{timeout:d.promise}));return e.then(function(){d=null},function(){d=null}),e.abort=function(a){d&&d.resolve("abort request: "+a)},e}var k=null,l=null,m="";if(location.search.indexOf("proxy=")>0)for(var n=location.search.replace("?","").split("&"),o=0;o<n.length;o++){var p=n[o].split("=");if("proxy"===p[0]){m="/proxy/"+p[1],"demo"==p[1]&&(m=Config.demo);break}}d("Authorization","Digest",{path:"/"});var q=null,r=null;return{url:function(){return m},mediaDemo:function(){return m==Config.demo?Config.demoMedia:!1},logUrl:function(a){return m+"/api/showLog"+(a||"")},authForMedia:function(){return d("auth")},authForRtsp:function(){return d("auth_rtsp")},previewUrl:function(a,b,c,d){return Config.allowDebugMode?"":m+"/api/image?physicalId="+a+(c?"&width="+c:"")+(d?"&height="+d:"")+("&time="+(b||"LATEST"))},hasProxy:function(){return""!==m},getUser:function(){var a=c.defer();return this.hasProxy()&&a.resolve(!1),this.getCurrentUser().then(function(b){var c=b.data.reply.isAdmin||b.data.reply.permissions&Config.globalEditServersPermissions;a.resolve({isAdmin:c,name:b.data.reply.name})}),a.promise},getScripts:function(){return a.get("/api/scriptList")},execute:function(b,c){return a.post("/api/execute/"+b+"?"+c)},getSettings:function(b){return b=b||m,b===!0?(k=null,f()):""===b?(null===k&&(k=h(f())),k):a.get(b+"/api/moduleInformation?showAddresses=true",{timeout:3e3})},saveSettings:function(a,b){return i(m+"/api/configure?systemName="+a+"&port="+b)},changePassword:function(a,b){return i(m+"/api/configure?password="+a+"&oldPassword="+b)},mergeSystems:function(a,b,c,d){return i(m+"/api/mergeSystems?password="+b+"&currentPassword="+c+"&url="+encodeURIComponent(a)+"&takeRemoteSettings="+!d)},pingSystem:function(a,b){return i(m+"/api/pingSystem?password="+b+"&url="+encodeURIComponent(a))},restart:function(){return i(m+"/api/restart")},getStorages:function(){return j(m+"/api/storageSpace")},saveStorages:function(a){return i(m+"/ec2/saveStorages",a)},discoveredPeers:function(){return j(m+"/api/discoveredPeers?showAddresses=true")},getMediaServer:function(a){return j(m+"/ec2/getMediaServersEx?id="+a.replace("{","").replace("}",""))},getMediaServers:function(){return j(m+"/ec2/getMediaServersEx")},getResourceTypes:function(){return j(m+"/ec2/getResourceTypes")},getLayouts:function(){return j(m+"/ec2/getLayouts")},getCameras:function(a){return j("undefined"!=typeof a?m+"/ec2/getCamerasEx?id="+a.replace("{","").replace("}",""):m+"/ec2/getCamerasEx")},saveMediaServer:function(a){return i(m+"/ec2/saveMediaServer",a)},statistics:function(a){return a=a||m,j(a+"/api/statistics?salt="+(new Date).getTime())},getCurrentUser:function(a){return(null===l||a)&&(l=j(m+"/api/getCurrentUser")),l},getTime:function(){return j(m+"/api/gettime")},logLevel:function(a,b){return j(m+"/api/logLevel?id="+a+(b?"&value="+b:""))},getRecords:function(a,b,c,d,e,f,g,h){var i=new Date;return"undefined"==typeof c&&(c=i.getTime()-2592e6),"undefined"==typeof d&&(d=i.getTime()+1e5),"undefined"==typeof e&&(e=(d-c)/1e3),"undefined"==typeof h&&(h=0),"/"!==a&&""!==a&&null!==a&&(a="/proxy/"+a+"/"),m==Config.demo&&(a=m+"/"),j(a+"ec2/recordedTimePeriods?"+(g||"")+"&physicalId="+b+"&startTime="+c+"&endTime="+d+"&detail="+e+"&periodsType="+h+(f?"&limit="+f:"")+"&flat")}}}]),angular.module("webadminApp").service("auth",["$http",function(a){this.login=function(b){return a.post("/ec2/login",{username:b.username,password:b.password})}}]),angular.module("webadminApp").directive("navbar",function(){return{restrict:"E",templateUrl:"views/navbar.html",controller:"NavigationCtrl",scope:{noPanel:"="}}}),angular.module("webadminApp").controller("NavigationCtrl",["$scope","$location","mediaserver","ipCookie","$sessionStorage",function(a,b,c,d,e){a.user={isAdmin:!0},c.getUser().then(function(b){a.user={isAdmin:b.isAdmin,name:b.name}}),c.getSettings().then(function(b){a.settings=b.data.reply,a.settings.remoteAddresses=a.settings.remoteAddresses.join("\n")}),a.isActive=function(a){var c=b.path().split("/")[1];return c.split("?")[0]===a.split("/")[1].split("?")[0]},a.logout=function(){d.remove("auth",{path:"/"}),d.remove("nonce",{path:"/"}),d.remove("realm",{path:"/"}),window.location.reload()},a.webclientEnabled=Config.webclientEnabled,a.alertVisible=!0,a.session=e,a.closeAlert=function(){a.session.serverInfoAlertHidden=!0}}]),angular.module("webadminApp").controller("LoginCtrl",["$scope","mediaserver","ipCookie","$sessionStorage",function(a,b,c,d){function e(){return a.authorized=!0,a.authorizing=!1,a.session.serverInfoAlertHidden=!1,setTimeout(function(){window.location.reload()},20),!1}a.authorized=!1,a.authorizing=!1,a.user||(a.user={username:"",password:""}),a.session=d,a.login=function(){if(a.loginForm.$valid){var d=a.user.username.toLowerCase(),f=c("realm"),g=c("nonce"),h=md5(d+":"+f+":"+a.user.password),i=md5("GET:"),j=md5(h+":"+g+":"+i),k=Base64.encode(d+":"+g+":"+j);c("auth",k,{path:"/"});var l=md5("PLAY:"),m=md5(h+":"+g+":"+l),n=Base64.encode(d+":"+g+":"+m);c("auth_rtsp",n,{path:"/"}),a.authorizing=!0,b.getCurrentUser(!0).then(e)["catch"](function(b){a.authorizing=!1,alert("Login or password is incorrect")})}}}]);