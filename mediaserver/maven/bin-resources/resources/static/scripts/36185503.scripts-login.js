"use strict";var Config={globalEditServersPermissions:32,demo:"/~ebalashov/webclient/api",demoMedia:"//10.0.2.186:7001",webclientEnabled:!0,helpLinks:[]};angular.module("webadminApp",["ipCookie","ngResource","ngSanitize","ngRoute","ui.bootstrap"]),angular.module("webadminApp").factory("mediaserver",["$http","$modal","$q","ipCookie","$log",function(a,b,c,d,e){function f(){return a.get(k+"/api/moduleInformation?showAddresses=true")}function g(a){if(401===a.status){var c=window.self!==window.top;return void(c||null!==p||(p=b.open({templateUrl:"views/login.html",keyboard:!1,backdrop:"static"}),p.result.then(function(){p=null})))}i=null,null===o&&f(!0).catch(function(a){})}function h(a){return a.catch(g),a}var i=null,j=null,k="";if(location.search.indexOf("proxy=")>0)for(var l=location.search.replace("?","").split("&"),m=0;m<l.length;m++){var n=l[m].split("=");if("proxy"===n[0]){k="/proxy/"+n[1],"demo"==n[1]&&(k=Config.demo);break}}d("Authorization","Digest",{path:"/"});var o=null,p=null;return{url:function(){return k},mediaDemo:function(){return k==Config.demo?Config.demoMedia:!1},logUrl:function(a){return k+"/api/showLog"+(a||"")},authForMedia:function(){return d("auth")},authForRtsp:function(){return d("auth_rtsp")},previewUrl:function(a,b,c,d){return k+"/api/image?physicalId="+a+(c?"&width="+c:"")+(d?"&height="+d:"")+("&time="+(b||"LATEST"))},hasProxy:function(){return""!==k},checkAdmin:function(){var a=c.defer();return this.hasProxy()&&a.resolve(!1),this.getCurrentUser().then(function(b){var c=b.data.reply.isAdmin||b.data.reply.permissions&Config.globalEditServersPermissions;a.resolve(c)}),a.promise},getSettings:function(b){return b=b||k,b===!0?(i=null,f()):""===b?(null===i&&(i=h(f())),i):a.get(b+"/api/moduleInformation?showAddresses=true",{timeout:3e3})},saveSettings:function(b,c){return h(a.post(k+"/api/configure?systemName="+b+"&port="+c))},changePassword:function(b,c){return h(a.post(k+"/api/configure?password="+b+"&oldPassword="+c))},mergeSystems:function(b,c,d,e){return h(a.post(k+"/api/mergeSystems?password="+c+"&currentPassword="+d+"&url="+encodeURIComponent(b)+"&takeRemoteSettings="+!e))},pingSystem:function(b,c){return h(a.post(k+"/api/pingSystem?password="+c+"&url="+encodeURIComponent(b)))},restart:function(){return h(a.post(k+"/api/restart"))},getStorages:function(){return h(a.get(k+"/api/storageSpace"))},saveStorages:function(b){return h(a.post(k+"/ec2/saveStorages",b))},discoveredPeers:function(){return h(a.get(k+"/api/discoveredPeers?showAddresses=true"))},getMediaServer:function(b){return h(a.get(k+"/ec2/getMediaServersEx?id="+b.replace("{","").replace("}","")))},getMediaServers:function(){return h(a.get(k+"/ec2/getMediaServersEx"))},getCameras:function(b){return h("undefined"!=typeof b?a.get(k+"/ec2/getCamerasEx?id="+b.replace("{","").replace("}","")):a.get(k+"/ec2/getCamerasEx"))},saveMediaServer:function(b){return h(a.post(k+"/ec2/saveMediaServer",b))},statistics:function(b){return b=b||k,h(a.get(b+"/api/statistics?salt="+(new Date).getTime()))},getCurrentUser:function(b){return(null===j||b)&&(j=h(a.get(k+"/api/getCurrentUser"))),j},getRecords:function(b,c,d,e,f,g,i){var j=new Date;return"undefined"==typeof d&&(d=j.getTime()-2592e6),"undefined"==typeof e&&(e=j.getTime()+1e5),"undefined"==typeof f&&(f=(e-d)/1e3),"undefined"==typeof i&&(i=0),"/"!==b&&""!==b&&null!==b&&(b="/proxy/"+b+"/"),k==Config.demo&&(b=k+"/"),h(a.get(b+"ec2/recordedTimePeriods?physicalId="+c+"&startTime="+d+"&endTime="+e+"&detail="+f+"&periodsType="+i+(g?"&limit="+g:"")+"&flat"))}}}]),angular.module("webadminApp").service("auth",["$http",function(a){this.login=function(b){return a.post("/ec2/login",{username:b.username,password:b.password})}}]),angular.module("webadminApp").directive("navbar",function(){return{restrict:"E",templateUrl:"views/navbar.html",controller:"NavigationCtrl",scope:{noPanel:"="}}}),angular.module("webadminApp").controller("NavigationCtrl",["$scope","$location","mediaserver","ipCookie",function(a,b,c,d){a.user={isAdmin:!0},c.checkAdmin().then(function(b){a.user={isAdmin:b}}),c.getSettings().then(function(b){a.settings=b.data.reply,a.settings.remoteAddresses=a.settings.remoteAddresses.join("\n")}),a.isActive=function(a){var c=b.path().split("/")[1];return c.split("?")[0]===a.split("/")[1].split("?")[0]},a.logout=function(){d.remove("response",{path:"/"}),d.remove("nonce",{path:"/"}),d.remove("realm",{path:"/"}),d.remove("username",{path:"/"}),window.location.reload()},a.webclientEnabled=Config.webclientEnabled,a.alertVisible=!0,a.closeAlert=function(){a.alertVisible=!1}}]),angular.module("webadminApp").controller("LoginCtrl",["$scope","mediaserver","ipCookie",function(a,b,c){function d(){return a.authorized=!0,a.authorizing=!1,setTimeout(function(){window.location.reload()},20),!1}a.authorized=!1,a.authorizing=!1,a.user||(a.user={username:"",password:""}),a.login=function(){if(a.loginForm.$valid){var e=a.user.username.toLowerCase(),f=c("realm"),g=c("nonce"),h=md5(e+":"+f+":"+a.user.password),i=md5("GET:"),j=md5(h+":"+g+":"+i),k=Base64.encode(e+":"+g+":"+j);c("auth",k,{path:"/"});var l=md5("PLAY:"),m=md5(h+":"+g+":"+l),n=Base64.encode(e+":"+g+":"+m);c("auth_rtsp",n,{path:"/"}),c("response",j,{path:"/"}),c("username",e,{path:"/"}),a.authorizing=!0,b.getCurrentUser(!0).then(d).catch(function(){a.authorizing=!1,alert("Login or password is incorrect")})}}}]);