// Copyright Google Inc. All Rights Reserved.
(function() { 'use strict';var f,aa="function"==typeof Object.create?Object.create:function(a){var b=function(){};b.prototype=a;return new b},ca;if("function"==typeof Object.setPrototypeOf)ca=Object.setPrototypeOf;else{var da;a:{var ea={Ta:!0},fa={};try{fa.__proto__=ea;da=fa.Ta;break a}catch(a){}da=!1}ca=da?function(a,b){a.__proto__=b;if(a.__proto__!==b)throw new TypeError(a+" is not extensible");return a}:null}
var ha=ca,g=function(a,b){a.prototype=aa(b.prototype);a.prototype.constructor=a;if(ha)ha(a,b);else for(var c in b)if("prototype"!=c)if(Object.defineProperties){var d=Object.getOwnPropertyDescriptor(b,c);d&&Object.defineProperty(a,c,d)}else a[c]=b[c];a.Kb=b.prototype},ia="function"==typeof Object.defineProperties?Object.defineProperty:function(a,b,c){a!=Array.prototype&&a!=Object.prototype&&(a[b]=c.value)},h="undefined"!=typeof window&&window===this?this:"undefined"!=typeof global&&null!=global?global:
this,ja=function(){ja=function(){};h.Symbol||(h.Symbol=ka)},ka=function(){var a=0;return function(b){return"jscomp_symbol_"+(b||"")+a++}}(),ma=function(){ja();var a=h.Symbol.iterator;a||(a=h.Symbol.iterator=h.Symbol("iterator"));"function"!=typeof Array.prototype[a]&&ia(Array.prototype,a,{configurable:!0,writable:!0,value:function(){return la(this)}});ma=function(){}},la=function(a){var b=0;return na(function(){return b<a.length?{done:!1,value:a[b++]}:{done:!0}})},na=function(a){ma();a={next:a};a[h.Symbol.iterator]=
function(){return this};return a},oa=function(a){ma();var b=a[Symbol.iterator];return b?b.call(a):la(a)},k=this,n=function(){},pa=function(a){var b=typeof a;if("object"==b)if(a){if(a instanceof Array)return"array";if(a instanceof Object)return b;var c=Object.prototype.toString.call(a);if("[object Window]"==c)return"object";if("[object Array]"==c||"number"==typeof a.length&&"undefined"!=typeof a.splice&&"undefined"!=typeof a.propertyIsEnumerable&&!a.propertyIsEnumerable("splice"))return"array";if("[object Function]"==
c||"undefined"!=typeof a.call&&"undefined"!=typeof a.propertyIsEnumerable&&!a.propertyIsEnumerable("call"))return"function"}else return"null";else if("function"==b&&"undefined"==typeof a.call)return"object";return b},qa=function(a){var b=pa(a);return"array"==b||"object"==b&&"number"==typeof a.length},p=function(a){return"function"==pa(a)},ra=function(a,b,c){return a.call.apply(a.bind,arguments)},sa=function(a,b,c){if(!a)throw Error();if(2<arguments.length){var d=Array.prototype.slice.call(arguments,
2);return function(){var c=Array.prototype.slice.call(arguments);Array.prototype.unshift.apply(c,d);return a.apply(b,c)}}return function(){return a.apply(b,arguments)}},r=function(a,b,c){r=Function.prototype.bind&&-1!=Function.prototype.bind.toString().indexOf("native code")?ra:sa;return r.apply(null,arguments)},ta=Date.now||function(){return+new Date},t=function(a,b){a=a.split(".");var c=k;a[0]in c||!c.execScript||c.execScript("var "+a[0]);for(var d;a.length&&(d=a.shift());)a.length||void 0===b?
c=c[d]&&c[d]!==Object.prototype[d]?c[d]:c[d]={}:c[d]=b},u=function(a,b){function c(){}c.prototype=b.prototype;a.Kb=b.prototype;a.prototype=new c;a.prototype.constructor=a;a.Ob=function(a,c,m){for(var d=Array(arguments.length-2),e=2;e<arguments.length;e++)d[e-2]=arguments[e];return b.prototype[c].apply(a,d)}};var v=function(a){if(Error.captureStackTrace)Error.captureStackTrace(this,v);else{var b=Error().stack;b&&(this.stack=b)}a&&(this.message=String(a))};u(v,Error);v.prototype.name="CustomError";var ua=function(a,b){for(var c=a.split("%s"),d="",e=Array.prototype.slice.call(arguments,1);e.length&&1<c.length;)d+=c.shift()+e.shift();return d+c.join("%s")},va=String.prototype.trim?function(a){return a.trim()}:function(a){return a.replace(/^[\s\xa0]+|[\s\xa0]+$/g,"")},wa=function(a,b){return a<b?-1:a>b?1:0};var xa=function(a,b){b.unshift(a);v.call(this,ua.apply(null,b));b.shift()};u(xa,v);xa.prototype.name="AssertionError";var ya=function(a,b){throw new xa("Failure"+(a?": "+a:""),Array.prototype.slice.call(arguments,1));};var w;a:{var za=k.navigator;if(za){var Aa=za.userAgent;if(Aa){w=Aa;break a}}w=""}var x=function(a){return-1!=w.indexOf(a)};var Ca=function(a,b){var c=Ba;return Object.prototype.hasOwnProperty.call(c,a)?c[a]:c[a]=b(a)};var Da=x("Opera"),y=x("Trident")||x("MSIE"),Ea=x("Edge"),Fa=x("Gecko")&&!(-1!=w.toLowerCase().indexOf("webkit")&&!x("Edge"))&&!(x("Trident")||x("MSIE"))&&!x("Edge"),Ga=-1!=w.toLowerCase().indexOf("webkit")&&!x("Edge"),Ha;
a:{var Ia="",Ja=function(){var a=w;if(Fa)return/rv\:([^\);]+)(\)|;)/.exec(a);if(Ea)return/Edge\/([\d\.]+)/.exec(a);if(y)return/\b(?:MSIE|rv)[: ]([^\);]+)(\)|;)/.exec(a);if(Ga)return/WebKit\/(\S+)/.exec(a);if(Da)return/(?:Version)[ \/]?(\S+)/.exec(a)}();Ja&&(Ia=Ja?Ja[1]:"");if(y){var z,Ka=k.document;z=Ka?Ka.documentMode:void 0;if(null!=z&&z>parseFloat(Ia)){Ha=String(z);break a}}Ha=Ia}
var La=Ha,Ba={},A=function(a){return Ca(a,function(){for(var b=0,c=va(String(La)).split("."),d=va(String(a)).split("."),e=Math.max(c.length,d.length),m=0;0==b&&m<e;m++){var l=c[m]||"",q=d[m]||"";do{l=/(\d*)(\D*)(.*)/.exec(l)||["","","",""];q=/(\d*)(\D*)(.*)/.exec(q)||["","","",""];if(0==l[0].length&&0==q[0].length)break;b=wa(0==l[1].length?0:parseInt(l[1],10),0==q[1].length?0:parseInt(q[1],10))||wa(0==l[2].length,0==q[2].length)||wa(l[2],q[2]);l=l[3];q=q[3]}while(0==b)}return 0<=b})};var B=function(a,b,c,d,e){this.reset(a,b,c,d,e)};B.prototype.ga=null;var Ma=0;B.prototype.reset=function(a,b,c,d,e){"number"==typeof e||Ma++;this.Sa=d||ta();this.A=a;this.Ha=b;this.Fa=c;delete this.ga};B.prototype.wa=function(a){this.A=a};var C=function(a){this.Ia=a;this.N=this.ea=this.A=this.j=null},D=function(a,b){this.name=a;this.value=b};D.prototype.toString=function(){return this.name};
var Na=new D("SHOUT",1200),E=new D("SEVERE",1E3),Oa=new D("WARNING",900),Pa=new D("INFO",800),Qa=new D("CONFIG",700),Ra=[new D("OFF",Infinity),Na,E,Oa,Pa,Qa,new D("FINE",500),new D("FINER",400),new D("FINEST",300),new D("ALL",0)],F=null,Sa=function(a){if(!F){F={};for(var b=0,c;c=Ra[b];b++)F[c.value]=c,F[c.name]=c}if(a in F)return F[a];for(b=0;b<Ra.length;++b)if(c=Ra[b],c.value<=a)return c;return null};C.prototype.getName=function(){return this.Ia};C.prototype.getParent=function(){return this.j};
C.prototype.wa=function(a){this.A=a};var Ta=function(a){if(a.A)return a.A;if(a.j)return Ta(a.j);ya("Root logger has no level set.");return null};C.prototype.log=function(a,b,c){if(a.value>=Ta(this).value)for(p(b)&&(b=b()),a=new B(a,String(b),this.Ia),c&&(a.ga=c),c="log:"+a.Ha,(b=k.console)&&b.timeStamp&&b.timeStamp(c),(b=k.msWriteProfilerMark)&&b(c),c=this;c;){var d=c,e=a;if(d.N)for(var m=0;b=d.N[m];m++)b(e);c=c.getParent()}};C.prototype.info=function(a,b){this.log(Pa,a,b)};
var Ua={},G=null,Va=function(){G||(G=new C(""),Ua[""]=G,G.wa(Qa))},Wa=function(){Va();return G},H=function(a){Va();var b;if(!(b=Ua[a])){b=new C(a);var c=a.lastIndexOf("."),d=a.substr(c+1);c=H(a.substr(0,c));c.ea||(c.ea={});c.ea[d]=b;b.j=c;Ua[a]=b}return b};var I=function(a){var b=Xa;b&&b.log(Oa,a,void 0)};var J=function(){this.Ma=ta()},Ya=null;J.prototype.set=function(a){this.Ma=a};J.prototype.reset=function(){this.set(ta())};J.prototype.get=function(){return this.Ma};var Za=function(a){this.yb=a||"";Ya||(Ya=new J);this.Jb=Ya};f=Za.prototype;f.ya=!0;f.Qa=!0;f.Hb=!0;f.Gb=!0;f.Ra=!1;f.Ib=!1;var K=function(a){return 10>a?"0"+a:String(a)},$a=function(a,b){a=(a.Sa-b)/1E3;b=a.toFixed(3);var c=0;if(1>a)c=2;else for(;100>a;)c++,a*=10;for(;0<c--;)b=" "+b;return b},ab=function(a){Za.call(this,a)};u(ab,Za);var bb=function(){this.zb=r(this.Ua,this);this.V=new ab;this.V.Qa=!1;this.V.Ra=!1;this.Ea=this.V.ya=!1;this.cb={}};
bb.prototype.Ua=function(a){if(!this.cb[a.Fa]){var b=this.V;var c=[];c.push(b.yb," ");if(b.Qa){var d=new Date(a.Sa);c.push("[",K(d.getFullYear()-2E3)+K(d.getMonth()+1)+K(d.getDate())+" "+K(d.getHours())+":"+K(d.getMinutes())+":"+K(d.getSeconds())+"."+K(Math.floor(d.getMilliseconds()/10)),"] ")}b.Hb&&c.push("[",$a(a,b.Jb.get()),"s] ");b.Gb&&c.push("[",a.Fa,"] ");b.Ib&&c.push("[",a.A.name,"] ");c.push(a.Ha);b.Ra&&(d=a.ga)&&c.push("\n",d instanceof Error?d.message:d.toString());b.ya&&c.push("\n");b=
c.join("");if(c=cb)switch(a.A){case Na:M(c,"info",b);break;case E:M(c,"error",b);break;case Oa:M(c,"warn",b);break;default:M(c,"log",b)}}};var N=null,cb=k.console,M=function(a,b,c){if(a[b])a[b](c);else a.log(c)};var db=function(a,b,c){this.pb=c;this.Ya=a;this.Cb=b;this.$=0;this.X=null};db.prototype.get=function(){if(0<this.$){this.$--;var a=this.X;this.X=a.next;a.next=null}else a=this.Ya();return a};db.prototype.put=function(a){this.Cb(a);this.$<this.pb&&(this.$++,a.next=this.X,this.X=a)};var eb=function(a){k.setTimeout(function(){throw a;},0)},fb,gb=function(){var a=k.MessageChannel;"undefined"===typeof a&&"undefined"!==typeof window&&window.postMessage&&window.addEventListener&&!x("Presto")&&(a=function(){var a=document.createElement("IFRAME");a.style.display="none";a.src="";document.documentElement.appendChild(a);var b=a.contentWindow;a=b.document;a.open();a.write("");a.close();var c="callImmediate"+Math.random(),d="file:"==b.location.protocol?"*":b.location.protocol+"//"+b.location.host;
a=r(function(a){if(("*"==d||a.origin==d)&&a.data==c)this.port1.onmessage()},this);b.addEventListener("message",a,!1);this.port1={};this.port2={postMessage:function(){b.postMessage(c,d)}}});if("undefined"!==typeof a&&!x("Trident")&&!x("MSIE")){var b=new a,c={},d=c;b.port1.onmessage=function(){if(void 0!==c.next){c=c.next;var a=c.za;c.za=null;a()}};return function(a){d.next={za:a};d=d.next;b.port2.postMessage(0)}}return"undefined"!==typeof document&&"onreadystatechange"in document.createElement("SCRIPT")?
function(a){var b=document.createElement("SCRIPT");b.onreadystatechange=function(){b.onreadystatechange=null;b.parentNode.removeChild(b);b=null;a();a=null};document.documentElement.appendChild(b)}:function(a){k.setTimeout(a,0)}};var hb=function(){this.ba=this.K=null},jb=new db(function(){return new ib},function(a){a.reset()},100);hb.prototype.add=function(a,b){var c=jb.get();c.set(a,b);this.ba?this.ba.next=c:this.K=c;this.ba=c};hb.prototype.remove=function(){var a=null;this.K&&(a=this.K,this.K=this.K.next,this.K||(this.ba=null),a.next=null);return a};var ib=function(){this.next=this.scope=this.ia=null};ib.prototype.set=function(a,b){this.ia=a;this.scope=b;this.next=null};
ib.prototype.reset=function(){this.next=this.scope=this.ia=null};var ob=function(a,b){kb||lb();mb||(kb(),mb=!0);nb.add(a,b)},kb,lb=function(){if(-1!=String(k.Promise).indexOf("[native code]")){var a=k.Promise.resolve(void 0);kb=function(){a.then(pb)}}else kb=function(){var a=pb;!p(k.setImmediate)||k.Window&&k.Window.prototype&&!x("Edge")&&k.Window.prototype.setImmediate==k.setImmediate?(fb||(fb=gb()),fb(a)):k.setImmediate(a)}},mb=!1,nb=new hb,pb=function(){for(var a;a=nb.remove();){try{a.ia.call(a.scope)}catch(b){eb(b)}jb.put(a)}mb=!1};var Q=function(a,b){this.g=0;this.Na=void 0;this.C=this.o=this.j=null;this.W=this.ha=!1;if(a!=n)try{var c=this;a.call(b,function(a){O(c,2,a)},function(a){if(!(a instanceof P))try{if(a instanceof Error)throw a;throw Error("Promise rejected.");}catch(e){}O(c,3,a)})}catch(d){O(this,3,d)}},qb=function(){this.next=this.context=this.G=this.P=this.w=null;this.U=!1};qb.prototype.reset=function(){this.context=this.G=this.P=this.w=null;this.U=!1};
var rb=new db(function(){return new qb},function(a){a.reset()},100),sb=function(a,b,c){var d=rb.get();d.P=a;d.G=b;d.context=c;return d},R=function(){var a,b,c=new Q(function(c,e){a=c;b=e});return new tb(c,a,b)};Q.prototype.then=function(a,b,c){return ub(this,p(a)?a:null,p(b)?b:null,c)};Q.prototype.then=Q.prototype.then;Q.prototype.$goog_Thenable=!0;Q.prototype.cancel=function(a){0==this.g&&ob(function(){var b=new P(a);vb(this,b)},this)};
var vb=function(a,b){if(0==a.g)if(a.j){var c=a.j;if(c.o){for(var d=0,e=null,m=null,l=c.o;l&&(l.U||(d++,l.w==a&&(e=l),!(e&&1<d)));l=l.next)e||(m=l);e&&(0==c.g&&1==d?vb(c,b):(m?(d=m,d.next==c.C&&(c.C=d),d.next=d.next.next):wb(c),xb(c,e,3,b)))}a.j=null}else O(a,3,b)},zb=function(a,b){a.o||2!=a.g&&3!=a.g||yb(a);a.C?a.C.next=b:a.o=b;a.C=b},ub=function(a,b,c,d){var e=sb(null,null,null);e.w=new Q(function(a,l){e.P=b?function(c){try{var e=b.call(d,c);a(e)}catch(L){l(L)}}:a;e.G=c?function(b){try{var e=c.call(d,
b);void 0===e&&b instanceof P?l(b):a(e)}catch(L){l(L)}}:l});e.w.j=a;zb(a,e);return e.w};Q.prototype.Lb=function(a){this.g=0;O(this,2,a)};Q.prototype.Mb=function(a){this.g=0;O(this,3,a)};
var O=function(a,b,c){if(0==a.g){a===c&&(b=3,c=new TypeError("Promise cannot resolve to itself"));a.g=1;a:{var d=c,e=a.Lb,m=a.Mb;if(d instanceof Q){zb(d,sb(e||n,m||null,a));var l=!0}else{if(d)try{var q=!!d.$goog_Thenable}catch(L){q=!1}else q=!1;if(q)d.then(e,m,a),l=!0;else{q=typeof d;if("object"==q&&null!=d||"function"==q)try{var ba=d.then;if(p(ba)){Ab(d,ba,e,m,a);l=!0;break a}}catch(L){m.call(a,L);l=!0;break a}l=!1}}}l||(a.Na=c,a.g=b,a.j=null,yb(a),3!=b||c instanceof P||Bb(a,c))}},Ab=function(a,
b,c,d,e){var m=!1,l=function(a){m||(m=!0,c.call(e,a))},q=function(a){m||(m=!0,d.call(e,a))};try{b.call(a,l,q)}catch(ba){q(ba)}},yb=function(a){a.ha||(a.ha=!0,ob(a.ab,a))},wb=function(a){var b=null;a.o&&(b=a.o,a.o=b.next,b.next=null);a.o||(a.C=null);return b};Q.prototype.ab=function(){for(var a;a=wb(this);)xb(this,a,this.g,this.Na);this.ha=!1};
var xb=function(a,b,c,d){if(3==c&&b.G&&!b.U)for(;a&&a.W;a=a.j)a.W=!1;if(b.w)b.w.j=null,Cb(b,c,d);else try{b.U?b.P.call(b.context):Cb(b,c,d)}catch(e){Db.call(null,e)}rb.put(b)},Cb=function(a,b,c){2==b?a.P.call(a.context,c):a.G&&a.G.call(a.context,c)},Bb=function(a,b){a.W=!0;ob(function(){a.W&&Db.call(null,b)})},Db=eb,P=function(a){v.call(this,a)};u(P,v);P.prototype.name="cancel";var tb=function(a,b,c){this.I=a;this.resolve=b;this.reject=c};var Eb=function(){this.Ba=this.Ba;this.vb=this.vb};Eb.prototype.Ba=!1;y&&A("9");!Ga||A("528");Fa&&A("1.9b")||y&&A("8")||Da&&A("9.5")||Ga&&A("528");Fa&&!A("8")||y&&A("9");(function(){if(!k.addEventListener||!Object.defineProperty)return!1;var a=!1,b=Object.defineProperty({},"passive",{get:function(){a=!0}});k.addEventListener("test",n,b);k.removeEventListener("test",n,b);return a})();var Fb=function(a,b,c){Eb.call(this);this.qb=null!=c?r(a,c):a;this.ob=b;this.Xa=r(this.xb,this);this.da=[]};u(Fb,Eb);f=Fb.prototype;f.J=!1;f.R=0;f.B=null;f.eb=function(a){this.da=arguments;this.B||this.R?this.J=!0:Gb(this)};f.stop=function(){this.B&&(k.clearTimeout(this.B),this.B=null,this.J=!1,this.da=[])};f.pause=function(){this.R++};f.resume=function(){this.R--;this.R||!this.J||this.B||(this.J=!1,Gb(this))};f.xb=function(){this.B=null;this.J&&!this.R&&(this.J=!1,Gb(this))};
var Gb=function(a){var b=a.Xa;var c=a.ob;if(!p(b))if(b&&"function"==typeof b.handleEvent)b=r(b.handleEvent,b);else throw Error("Invalid listener argument");b=2147483647<Number(c)?-1:k.setTimeout(b,c||0);a.B=b;a.qb.apply(null,a.da)};var S=function(a){a.controller=this;this.a=a;this.u=this.f=this.b=null;this.Ka=this.wb.bind(this);this.D=this.sb.bind(this);this.F=this.tb.bind(this);this.m=0;this.Nb=new Fb(this.bb,200,this)};f=S.prototype;f.sa=function(){this.f&&(this.m++,this.a.isPaused=!this.a.isPaused,this.a.isPaused?this.f.pause(null,this.F,this.D):this.f.play(null,this.F,this.D))};f.stop=function(){this.f&&(this.m++,this.f.stop(null,this.F,this.D))};
f.seek=function(){if(this.f){this.u&&(clearTimeout(this.u),this.u=null);this.m++;var a=new chrome.cast.media.SeekRequest;a.currentTime=this.a.currentTime;this.f.seek(a,this.F,this.D)}};f.qa=function(){this.b&&(this.m++,this.a.isMuted=!this.a.isMuted,this.b.setReceiverMuted(this.a.isMuted,this.F,this.D))};f.xa=function(){this.Nb.eb()};f.bb=function(){this.b&&(this.m++,this.b.setReceiverVolumeLevel(this.a.volumeLevel,this.F,this.D))};f.tb=function(){this.m--;this.v()};
f.sb=function(){this.m--;this.f&&this.f.getStatus(null,n,n)};f.wb=function(){this.f&&(this.a.currentTime=this.f.getEstimatedTime(),this.u=setTimeout(this.Ka,1E3))};
f.v=function(a){if(!(0<this.m))if(this.b){this.a.displayName=this.b.displayName||"";var b=this.b.statusText||"";this.a.displayStatus=b!=this.a.displayName?b:"";!a&&this.b.receiver&&(a=this.b.receiver.volume)&&(null!=a.muted&&(this.a.isMuted=a.muted),null!=a.level&&(this.a.volumeLevel=a.level),this.a.canControlVolume="fixed"!=a.controlType);this.f?(this.a.isMediaLoaded=this.f.playerState!=chrome.cast.media.PlayerState.IDLE,this.a.isPaused=this.f.playerState==chrome.cast.media.PlayerState.PAUSED,this.a.canPause=
0<=this.f.supportedMediaCommands.indexOf(chrome.cast.media.MediaCommand.PAUSE),this.T(this.f.media),this.a.canSeek=0<=this.f.supportedMediaCommands.indexOf(chrome.cast.media.MediaCommand.SEEK)&&0!=this.a.duration,this.a.currentTime=this.f.getEstimatedTime(),this.u&&(clearTimeout(this.u),this.u=null),this.f.playerState==chrome.cast.media.PlayerState.PLAYING&&(this.u=setTimeout(this.Ka,1E3))):this.T(null)}else this.a.displayName="",this.a.displayStatus="",this.T(null)};
f.T=function(a){a?(this.a.duration=a.duration||0,a.metadata&&a.metadata.title&&(this.a.displayStatus=a.metadata.title)):(this.a.isMediaLoaded=!1,this.a.canPause=!1,this.a.canSeek=!1,this.a.currentTime=0,this.a.duration=0)};var Hb=function(a){if(!a.f)for(var b=0,c=a.b.media;b<c.length;b++)if(!c[b].idleReason){a.f=c[b];a.f.addUpdateListener(a.rb.bind(a));break}},Ib=function(a,b){a.b=b;b.addMediaListener(a.Ga.bind(a));b.addUpdateListener(a.va.bind(a));Hb(a);a.v()};f=S.prototype;
f.va=function(a){a||(this.f=this.b=null);this.v()};f.Ga=function(){Hb(this);this.v(!0)};f.rb=function(a){a||(this.f=null);this.v(!0)};f.ka=function(a,b){return b?100*a/b:0};f.la=function(a,b){return b?a*b/100:0};f.ja=function(a){return 0>a?"":[("0"+Math.floor(a/3600)).substr(-2),("0"+Math.floor(a/60)%60).substr(-2),("0"+Math.floor(a)%60).substr(-2)].join(":")};var Jb=H("castx.common.loadScript"),Kb=function(){new Promise(function(a,b){var c=document.createElement("script");c.type="text/javascript";c.src="https://www.gstatic.com/external_hosted/polymer/v1/webcomponents.min.js";c.onload=function(){Jb&&Jb.info("library(https://www.gstatic.com/external_hosted/polymer/v1/webcomponents.min.js is loaded",void 0);a()};c.onerror=function(){Jb&&Jb.log(E,"library(https://www.gstatic.com/external_hosted/polymer/v1/webcomponents.min.js) failed to load",void 0);b()};
(document.head||document.body||document).appendChild(c)})};t("cast.framework.VERSION","1.0.06");t("cast.framework.LoggerLevel",{DEBUG:0,INFO:800,WARNING:900,ERROR:1E3,NONE:1500});t("cast.framework.CastState",{NO_DEVICES_AVAILABLE:"NO_DEVICES_AVAILABLE",NOT_CONNECTED:"NOT_CONNECTED",CONNECTING:"CONNECTING",CONNECTED:"CONNECTED"});
t("cast.framework.SessionState",{NO_SESSION:"NO_SESSION",SESSION_STARTING:"SESSION_STARTING",SESSION_STARTED:"SESSION_STARTED",SESSION_START_FAILED:"SESSION_START_FAILED",SESSION_ENDING:"SESSION_ENDING",SESSION_ENDED:"SESSION_ENDED",SESSION_RESUMED:"SESSION_RESUMED"});t("cast.framework.CastContextEventType",{CAST_STATE_CHANGED:"caststatechanged",SESSION_STATE_CHANGED:"sessionstatechanged"});
t("cast.framework.SessionEventType",{APPLICATION_STATUS_CHANGED:"applicationstatuschanged",APPLICATION_METADATA_CHANGED:"applicationmetadatachanged",ACTIVE_INPUT_STATE_CHANGED:"activeinputstatechanged",VOLUME_CHANGED:"volumechanged",MEDIA_SESSION:"mediasession"});
t("cast.framework.RemotePlayerEventType",{ANY_CHANGE:"anyChanged",IS_CONNECTED_CHANGED:"isConnectedChanged",IS_MEDIA_LOADED_CHANGED:"isMediaLoadedChanged",DURATION_CHANGED:"durationChanged",CURRENT_TIME_CHANGED:"currentTimeChanged",IS_PAUSED_CHANGED:"isPausedChanged",VOLUME_LEVEL_CHANGED:"volumeLevelChanged",CAN_CONTROL_VOLUME_CHANGED:"canControlVolumeChanged",IS_MUTED_CHANGED:"isMutedChanged",CAN_PAUSE_CHANGED:"canPauseChanged",CAN_SEEK_CHANGED:"canSeekChanged",DISPLAY_NAME_CHANGED:"displayNameChanged",
STATUS_TEXT_CHANGED:"statusTextChanged",TITLE_CHANGED:"titleChanged",DISPLAY_STATUS_CHANGED:"displayStatusChanged",MEDIA_INFO_CHANGED:"mediaInfoChanged",IMAGE_URL_CHANGED:"imageUrlChanged",PLAYER_STATE_CHANGED:"playerStateChanged"});t("cast.framework.ActiveInputState",{ACTIVE_INPUT_STATE_UNKNOWN:-1,ACTIVE_INPUT_STATE_NO:0,ACTIVE_INPUT_STATE_YES:1});var Lb=function(a){Wa().wa(Sa(a))};t("cast.framework.setLoggerLevel",Lb);N||(N=new bb);
if(N){var Mb=N;if(1!=Mb.Ea){var Nb=Wa(),Ob=Mb.zb;Nb.N||(Nb.N=[]);Nb.N.push(Ob);Mb.Ea=!0}}Lb(1E3);var T=function(a){this.type=a};t("cast.framework.EventData",T);var Pb=function(a){this.type="activeinputstatechanged";this.activeInputState=a};g(Pb,T);t("cast.framework.ActiveInputStateEventData",Pb);var Qb=function(a){this.applicationId=a.appId;this.name=a.displayName;this.images=a.appImages;this.namespaces=this.ra(a.namespaces||[])};t("cast.framework.ApplicationMetadata",Qb);Qb.prototype.ra=function(a){return a.map(function(a){return a.name})};var Rb=function(a){this.type="applicationmetadatachanged";this.metadata=a};g(Rb,T);t("cast.framework.ApplicationMetadataEventData",Rb);var Sb=function(a){this.type="applicationstatuschanged";this.status=a};g(Sb,T);t("cast.framework.ApplicationStatusEventData",Sb);var Tb=H("cast.framework.EventTarget"),U=function(){this.O={}};U.prototype.addEventListener=function(a,b){a in this.O||(this.O[a]=[]);a=this.O[a];a.includes(b)||a.push(b)};U.prototype.removeEventListener=function(a,b){a=this.O[a]||[];b=a.indexOf(b);0<=b&&a.splice(b,1)};U.prototype.dispatchEvent=function(a){a&&a.type&&(this.O[a.type]||[]).forEach(function(b){try{b(a)}catch(c){Tb&&Tb.log(E,"Handler for "+a.type+" event failed: "+c,c)}})};var Ub=function(a){a=a||{};this.receiverApplicationId=a.receiverApplicationId||null;this.resumeSavedSession=void 0!==a.resumeSavedSession?a.resumeSavedSession:!0;this.autoJoinPolicy=void 0!==a.autoJoinPolicy?a.autoJoinPolicy:chrome.cast.AutoJoinPolicy.TAB_AND_ORIGIN_SCOPED;this.language=a.language||null};t("cast.framework.CastOptions",Ub);var Vb=function(a){this.type="mediasession";this.mediaSession=a};g(Vb,T);t("cast.framework.MediaSessionEventData",Vb);var Wb=function(a,b){this.type="volumechanged";this.volume=a;this.isMute=b};g(Wb,T);t("cast.framework.VolumeEventData",Wb);var V=function(a,b){this.h=new U;this.g=b;this.c=a;this.Oa=a.sessionId;this.S=a.statusText;this.La=a.receiver;this.i=a.receiver.volume;this.Z=new Qb(a);this.Y=a.receiver.isActiveInput;this.c.addMediaListener(this.pa.bind(this));Xb(this)};t("cast.framework.CastSession",V);var Xb=function(a){var b=a.c.loadMedia.bind(a.c);a.c.loadMedia=function(c,e,m){b(c,function(b){e&&e(b);a.pa(b)},m)};var c=a.c.queueLoad.bind(a.c);a.c.queueLoad=function(b,e,m){c(b,function(b){e&&e(b);a.pa(b)},m)}};
V.prototype.addEventListener=function(a,b){this.h.addEventListener(a,b)};V.prototype.addEventListener=V.prototype.addEventListener;V.prototype.removeEventListener=function(a,b){this.h.removeEventListener(a,b)};V.prototype.removeEventListener=V.prototype.removeEventListener;var Zb=function(a,b){a.La=b;!b.volume||a.i&&a.i.muted==b.volume.muted&&a.i.level==b.volume.level||(a.i=b.volume,a.h.dispatchEvent(new Wb(a.i.level,a.i.muted)));a.Y!=b.isActiveInput&&(a.Y=b.isActiveInput,a.h.dispatchEvent(new Pb(Yb(a.Y))))};
V.prototype.mb=function(){return this.c};V.prototype.getSessionObj=V.prototype.mb;V.prototype.lb=function(){return this.Oa};V.prototype.getSessionId=V.prototype.lb;V.prototype.ma=function(){return this.g};V.prototype.getSessionState=V.prototype.ma;V.prototype.ib=function(){return this.La};V.prototype.getCastDevice=V.prototype.ib;V.prototype.gb=function(){return this.Z};V.prototype.getApplicationMetadata=V.prototype.gb;V.prototype.hb=function(){return this.S};V.prototype.getApplicationStatus=V.prototype.hb;
V.prototype.fb=function(){return Yb(this.Y)};V.prototype.getActiveInputState=V.prototype.fb;V.prototype.Ca=function(a){"SESSION_ENDED"!=this.g&&(a?this.c.stop(n,n):this.c.leave(n,n))};V.prototype.endSession=V.prototype.Ca;V.prototype.setVolume=function(a){var b=R(),c=Promise.resolve(b.I);this.i&&(this.i.level=a,this.i.muted=!1);this.c.setReceiverVolumeLevel(a,function(){return b.resolve()},function(a){return b.reject(a.code)});return c};V.prototype.setVolume=V.prototype.setVolume;
V.prototype.nb=function(){return this.i?this.i.level:null};V.prototype.getVolume=V.prototype.nb;V.prototype.Eb=function(a){var b=R(),c=Promise.resolve(b.I);this.i&&(this.i.muted=a);this.c.setReceiverMuted(a,function(){return b.resolve()},function(a){return b.reject(a.code)});return c};V.prototype.setMute=V.prototype.Eb;V.prototype.isMute=function(){return this.i?this.i.muted:null};V.prototype.isMute=V.prototype.isMute;
V.prototype.sendMessage=function(a,b){var c=R(),d=Promise.resolve(c.I);this.c.sendMessage(a,b,function(){return c.resolve()},function(a){return c.reject(a.code)});return d};V.prototype.sendMessage=V.prototype.sendMessage;V.prototype.addMessageListener=function(a,b){this.c.addMessageListener(a,b)};V.prototype.addMessageListener=V.prototype.addMessageListener;V.prototype.removeMessageListener=function(a,b){this.c.removeMessageListener(a,b)};V.prototype.removeMessageListener=V.prototype.removeMessageListener;
V.prototype.loadMedia=function(a){var b=R(),c=Promise.resolve(b.I);this.c.loadMedia(a,function(){b.resolve()},function(a){b.reject(a.code)});return c};V.prototype.loadMedia=V.prototype.loadMedia;V.prototype.Da=function(){a:{var a=this.c;if(a.media){a=oa(a.media);for(var b=a.next();!b.done;b=a.next())if(b=b.value,!b.idleReason){a=b;break a}}a=null}return a};V.prototype.getMediaSession=V.prototype.Da;V.prototype.pa=function(a){a.media&&this.h.dispatchEvent(new Vb(a))};
var Yb=function(a){return null==a?-1:a?1:0};V.prototype.ra=function(a){return a.map(function(a,c){return c.name})};var $b=function(a){this.type="caststatechanged";this.castState=a};g($b,T);t("cast.framework.CastStateEventData",$b);var ac=function(a,b,c){this.type="sessionstatechanged";this.session=a;this.sessionState=b;this.errorCode=void 0!==c?c:null};g(ac,T);t("cast.framework.SessionStateEventData",ac);var Xa=H("cast.framework.CastContext"),W=function(){this.h=new U;this.oa=!1;this.s=null;this.ua=!1;this.L="NO_DEVICES_AVAILABLE";this.l="NO_SESSION";this.aa=this.b=null};t("cast.framework.CastContext",W);W.prototype.addEventListener=function(a,b){this.h.addEventListener(a,b)};W.prototype.addEventListener=W.prototype.addEventListener;W.prototype.removeEventListener=function(a,b){this.h.removeEventListener(a,b)};W.prototype.removeEventListener=W.prototype.removeEventListener;
W.prototype.Fb=function(a){if(this.oa)I("CastContext already initialized, new options are ignored");else{this.s=new Ub(a);if(!this.s||!this.s.receiverApplicationId)throw Error("Missing application id in cast options");a=new chrome.cast.SessionRequest(this.s.receiverApplicationId);this.s.language&&(a.language=this.s.language);a=new chrome.cast.ApiConfig(a,this.Pa.bind(this),this.Bb.bind(this),this.s.autoJoinPolicy);chrome.cast.initialize(a,n,n);chrome.cast.addReceiverActionListener(this.Ab.bind(this));
this.oa=!0}};W.prototype.setOptions=W.prototype.Fb;W.prototype.jb=function(){return this.L};W.prototype.getCastState=W.prototype.jb;W.prototype.ma=function(){return this.l};W.prototype.getSessionState=W.prototype.ma;
W.prototype.requestSession=function(){var a=this;if(!this.oa)throw Error("Cannot start session before cast options are provided");var b=R(),c=Promise.resolve(b.I);ub(b.I,null,n,void 0);c.catch(n);var d="NOT_CONNECTED"==this.L;chrome.cast.requestSession(function(c){a.Pa(c);b.resolve(null)},function(c){d&&X(a,"SESSION_START_FAILED",c?c.code:void 0);b.reject(c.code)});return c};W.prototype.requestSession=W.prototype.requestSession;W.prototype.kb=function(){return this.b};
W.prototype.getCurrentSession=W.prototype.kb;W.prototype.$a=function(a){this.b&&this.b.Ca(a)};W.prototype.endCurrentSession=W.prototype.$a;W.prototype.Bb=function(a){(this.ua=a==chrome.cast.ReceiverAvailability.AVAILABLE)&&!this.b&&this.aa&&this.s.resumeSavedSession&&chrome.cast.requestSessionById(this.aa);bc(this)};W.prototype.Ab=function(a,b){this.b||b!=chrome.cast.ReceiverAction.CAST?this.b&&b==chrome.cast.ReceiverAction.STOP?X(this,"SESSION_ENDING"):a&&Zb(this.b,a):X(this,"SESSION_STARTING")};
W.prototype.Pa=function(a){var b="SESSION_STARTING"==this.l?"SESSION_STARTED":"SESSION_RESUMED";this.aa=null;this.b=new V(a,b);a.addUpdateListener(this.va.bind(this));X(this,b)};
W.prototype.va=function(){if(this.b)switch(this.b.c.status){case chrome.cast.SessionStatus.DISCONNECTED:case chrome.cast.SessionStatus.STOPPED:X(this,"SESSION_ENDED");this.aa=this.b.Oa;this.b=null;break;case chrome.cast.SessionStatus.CONNECTED:var a=this.b,b=a.Z,c=a.c,d;if(!(d=b.applicationId!=c.appId||b.name!=c.displayName)){a:if(d=b.namespaces,b=b.ra(c.namespaces||[]),qa(d)&&qa(b)&&d.length==b.length){c=d.length;for(var e=0;e<c;e++)if(d[e]!==b[e]){d=!1;break a}d=!0}else d=!1;d=!d}d&&(a.Z=new Qb(a.c),
a.h.dispatchEvent(new Rb(a.Z)));Zb(a,a.c.receiver);a.S!=a.c.statusText&&(a.S=a.c.statusText,a.h.dispatchEvent(new Sb(a.S)));break;default:I("Unknown session status "+this.b.c.status)}else I("Received session update event without known session")};
var X=function(a,b,c){b==a.l?"SESSION_START_FAILED"==b&&a.h.dispatchEvent(new ac(a.b,a.l,c)):(a.l=b,a.b&&(a.b.g=a.l),a.h.dispatchEvent(new ac(a.b,a.l,c)),bc(a))},bc=function(a){var b="NO_DEVICES_AVAILABLE";switch(a.l){case "SESSION_STARTING":case "SESSION_ENDING":b="CONNECTING";break;case "SESSION_STARTED":case "SESSION_RESUMED":b="CONNECTED";break;case "NO_SESSION":case "SESSION_ENDED":case "SESSION_START_FAILED":b=a.ua?"NOT_CONNECTED":"NO_DEVICES_AVAILABLE";break;default:I("Unexpected session state: "+
a.l)}b!==a.L&&(a.L=b,a.h.dispatchEvent(new $b(b)))};W.na=void 0;W.M=function(){return W.na?W.na:W.na=new W};W.getInstance=W.M;var Y=function(){return HTMLButtonElement.call(this)||this};g(Y,HTMLButtonElement);Y.prototype.createdCallback=function(){this.createShadowRoot&&(this.createShadowRoot().innerHTML='<style>.connected {fill:var(--connected-color,#4285F4);}.disconnected {fill:var(--disconnected-color,#7D7D7D);}.hidden {opacity:0;}</style><svg id "svg" x="0px" y="0px" width="100%" height="100%" viewBox="0 0 24 24"><g><path id="arch0" class="disconnected" d="M1,18 L1,21 L4,21 C4,19.34 2.66,18 1,18 L1,18 Z"/><path id="arch1" class="disconnected" d="M1,14 L1,16 C3.76,16 6,18.24 6,21 L8,21 C8,17.13 4.87,14 1,14 L1,14 Z"/><path id="arch2" class="disconnected" d="M1,10 L1,12 C5.97,12 10,16.03 10,21 L12,21 C12,14.92 7.07,10 1,10 L1,10 Z"/><path id="box" class="disconnected" d="M21,3 L3,3 C1.9,3 1,3.9 1,5 L1,8 L3,8 L3,5 L21,5 L21,19 L14,19 L14,21 L21,21 C22.1,21 23,20.1 23,19 L23,5 C23,3.9 22.1,3 21,3 L21,3 Z"/><path id="boxfill" class="hidden" d="M5,7 L5,8.63 C8,8.63 13.37,14 13.37,17 L19,17 L19,7 Z"/></g></svg>')};
Y.prototype.attachedCallback=function(){if(this.shadowRoot){this.fa=W.M();this.Ja=this.ub.bind(this);this.ca=[];for(var a=0;3>a;a++)this.ca.push(this.shadowRoot.getElementById("arch"+a));this.Va=this.shadowRoot.getElementById("box");this.Wa=this.shadowRoot.getElementById("boxfill");this.ta=0;this.H=null;this.Za=window.getComputedStyle(this,null).display;this.g=this.fa.L;cc(this);this.addEventListener("click",dc);this.fa.addEventListener("caststatechanged",this.Ja)}};
Y.prototype.detachedCallback=function(){this.fa.removeEventListener("caststatechanged",this.Ja);null!==this.H&&(window.clearTimeout(this.H),this.H=null)};var dc=function(){W.M().requestSession()};Y.prototype.ub=function(a){this.g=a.castState;cc(this)};var cc=function(a){if("NO_DEVICES_AVAILABLE"==a.g)a.style.display="none";else switch(a.style.display=a.Za,a.g){case "NOT_CONNECTED":ec(a,!1,"hidden");break;case "CONNECTING":ec(a,!1,"hidden");a.H||a.Aa();break;case "CONNECTED":ec(a,!0,"connected")}};
Y.prototype.Aa=function(){this.H=null;if("CONNECTING"==this.g){for(var a=0;3>a;a++)fc(this.ca[a],a==this.ta);this.ta=(this.ta+1)%3;this.H=window.setTimeout(this.Aa.bind(this),300)}};var ec=function(a,b,c){for(var d=oa(a.ca),e=d.next();!e.done;e=d.next())fc(e.value,b);fc(a.Va,b);a.Wa.setAttribute("class",c)},fc=function(a,b){a.setAttribute("class",b?"connected":"disconnected")};
(function(){var a=function(){document.registerElement("google-cast-button",{prototype:Y.prototype,extends:"button"})};if(document.registerElement)a();else{window.addEventListener("WebComponentsReady",a);Kb();a=oa(document.querySelectorAll("button[is=google-cast-button]"));for(var b=a.next();!b.done;b=a.next())b.value.onclick=dc}})();t("cast.framework.RemotePlayer",function(){this.isMediaLoaded=this.isConnected=!1;this.currentTime=this.duration=0;this.volumeLevel=1;this.canControlVolume=!0;this.canSeek=this.canPause=this.isMuted=this.isPaused=!1;this.displayStatus=this.title=this.statusText=this.displayName="";this.controller=this.savedPlayerState=this.playerState=this.imageUrl=this.mediaInfo=null});var gc=function(a,b,c){this.type=a;this.field=b;this.value=c};g(gc,T);t("cast.framework.RemotePlayerChangedEvent",gc);var Z=function(a){var b=new U;S.call(this,hc(a,b));this.h=b;a=W.M();a.addEventListener("sessionstatechanged",this.Db.bind(this));(a=a.b)?Ib(this,a.c):this.v()};g(Z,S);t("cast.framework.RemotePlayerController",Z);var hc=function(a,b){return new window.Proxy(a,{set:function(a,d,e){if(e===a[d])return!0;a[d]=e;b.dispatchEvent(new gc(d+"Changed",d,e));b.dispatchEvent(new gc("anyChanged",d,e));return!0}})};Z.prototype.addEventListener=function(a,b){this.h.addEventListener(a,b)};
Z.prototype.addEventListener=Z.prototype.addEventListener;Z.prototype.removeEventListener=function(a,b){this.h.removeEventListener(a,b)};Z.prototype.removeEventListener=Z.prototype.removeEventListener;Z.prototype.Db=function(a){switch(a.sessionState){case "SESSION_STARTED":case "SESSION_RESUMED":this.a.isConnected=!0;var b=a.session&&a.session.c;b&&(Ib(this,b),a.session.addEventListener("mediasession",this.Ga.bind(this)))}};
Z.prototype.v=function(a){var b=W.M().b;b?this.a.savedPlayerState=null:this.a.isConnected&&(this.a.savedPlayerState={mediaInfo:this.a.mediaInfo,currentTime:this.a.currentTime,isPaused:this.a.isPaused});S.prototype.v.call(this,a);this.a.isConnected=!!b;this.a.statusText=b&&b.S||"";a=b&&b.Da();this.a.playerState=a&&a.playerState||null};
Z.prototype.T=function(a){S.prototype.T.call(this,a);var b=(this.a.mediaInfo=a)&&a.metadata;a=null;var c="";b&&(c=b.title||"",(b=b.images)&&0<b.length&&(a=b[0].url));this.a.title=c;this.a.imageUrl=a};Z.prototype.sa=function(){S.prototype.sa.call(this)};Z.prototype.playOrPause=Z.prototype.sa;Z.prototype.stop=function(){S.prototype.stop.call(this)};Z.prototype.stop=Z.prototype.stop;Z.prototype.seek=function(){S.prototype.seek.call(this)};Z.prototype.seek=Z.prototype.seek;Z.prototype.qa=function(){S.prototype.qa.call(this)};
Z.prototype.muteOrUnmute=Z.prototype.qa;Z.prototype.xa=function(){S.prototype.xa.call(this)};Z.prototype.setVolumeLevel=Z.prototype.xa;Z.prototype.ja=function(a){return S.prototype.ja.call(this,a)};Z.prototype.getFormattedTime=Z.prototype.ja;Z.prototype.ka=function(a,b){return S.prototype.ka.call(this,a,b)};Z.prototype.getSeekPosition=Z.prototype.ka;Z.prototype.la=function(a,b){return S.prototype.la.call(this,a,b)};Z.prototype.getSeekTime=Z.prototype.la; }).call(window);
