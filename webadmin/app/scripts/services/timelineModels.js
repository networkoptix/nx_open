'use strict';


//Record
function Chunk(boundaries,start,end,level,title,extension){
    this.start = start;
    this.end = end;
    this.level = level || 0;
    this.expand = true;

    var format = 'dd.mm.yyyy HH:MM';
    this.title = (typeof(title) === 'undefined' || title === null) ? dateFormat(start,format) + ' - ' + dateFormat(end,format):title ;

    //console.log("create chunk ",this.level, this.title);

    this.children = [];

    _.extend(this, extension);
}



//Вспопомогательная мини-библиотека для декларации и использования настроек линейки
function Interval (seconds,minutes,hours,days,months,years){
    this.seconds = seconds;
    this.minutes = minutes;
    this.hours = hours;
    this.days = days;
    this.months = months;
    this.years = years;
}

// Прибавить интервал к дате
Interval.prototype.addToDate = function(date, count){
    if(Number.isInteger(date)) {
        date = new Date(date);
    }
    if(typeof(count)==='undefined') {
        count = 1;
    }
    try {
        return new Date(date.getFullYear() + count * this.years,
                date.getMonth() + count * this.months,
                date.getDate() + count * this.days,
                date.getHours() + count * this.hours,
                date.getMinutes() + count * this.minutes,
                date.getSeconds() + count * this.seconds);
    }catch(error){
        console.log(date);
        throw error;
    }
};

Interval.secondsBetweenDates = function(date1,date2){
    return (date2.getTime() - date1.getTime())/1000;
};
// Сколько секунд в интервале. Может выдавать не очень точный показатель, если интервал больше года из-за высокосности - на таком масштабе эта погрешность не играет роли
Interval.prototype.getSeconds = function(){
    var date1 = new Date(1971,11,1,0,0,0,0);//Первое декабря перед высокосным годом. Это нужно, чтобы интервал оценивался по максимуму (если месяц, то 31 день, если год - то 366 дней)
    var date2 = this.addToDate(date1);
    return Interval.secondsBetweenDates(date1,date2);
};
//Выравнивание даты по интервалу в прошлое
//Работает исходя из предположения, что не бывает интервала, состоящего из 1 месяца и 3х дней, например - значащий показатель в интервале только один
Interval.prototype.alignToPast = function(dateToAlign){
    var date = new Date(dateToAlign);

    date.setMilliseconds(0);
    if(this.seconds === 0){
        date.setSeconds(0);
    }else{
        date.setSeconds( Math.floor(date.getSeconds() / this.seconds) * this.seconds );
        return date;
    }
    if(this.minutes === 0){
        date.setMinutes(0);
    }else{
        date.setMinutes( Math.floor(date.getMinutes() / this.minutes) * this.minutes );
        return date;
    }
    if(this.hours === 0){
        date.setHours(0);
    }else{
        date.setHours( Math.floor(date.getHours() / this.hours) * this.hours );
        return date;
    }
    if(this.days === 0){
        date.setDate(1);
    }else{
        date.setDate( Math.floor(date.getDate() / this.days) * this.days );
        return date;
    }
    if(this.months === 0){
        date.setMonth(0);
    }else{
        date.setMonth( Math.floor(date.getMonth() / this.months) * this.months );
        return date;
    }
    date.setYear( Math.floor(date.getFullYear() / this.years) * this.years );
    return date;
};


//Check if current date aligned by interval
Interval.prototype.checkDate = function(date){


    if(Number.isInteger(date)) {
        date = new Date(date);
    }

    return this.alignToPast(date).getTime() === date.getTime();
};
//Выравнивание даты по интервалу в будущее
//Работает просто: добавляет интервал и выравнивает в прошлое
Interval.prototype.alignToFuture = function(date){
    return this.alignToPast(this.addToDate(date));
};


//Модель для линейки - дерево с возможностью детализации вглубь
var RulerModel = {

    /**
     * Presets for detailization levels
     * @type {{detailization: number}[]}
     */
    levels: [
        { name:'Age'        , format:'yyyy'     , interval:  new Interval( 0, 0, 0, 0, 0, 100), width: 25, topWidth: 0, topFormat:'' }, // root
        { name:'Decade'     , format:'yyyy'     , interval:  new Interval( 0, 0, 0, 0, 0, 10) , width: 25 },
        {
            name:'Year', //Years
            format:'yyyy',//Format string for date
            interval:  new Interval(0,0,0,0,0,1),// Interval for marks
            width: 25, // Minimal width for label. We should choose minimal width in order to not intresect labels on timeline
            topWidth: 100, // minimal width for label above timeline
            topFormat:'yyyy'//Format string for label above timeline
        },
        { name:'Month'      , format:'mmmm'     , interval:  new Interval( 0, 0, 0, 0, 1, 0)  , width: 50, topWidth: 140, topFormat:'mmmm yyyy'},
        { name:'Day'        , format:'dd mmmm'  , interval:  new Interval( 0, 0, 0, 1, 0, 0)  , width: 60, topWidth: 140, topFormat:'dd mmmm yyyy' },
        { name:'6 hours'    , format:'HH"h"'    , interval:  new Interval( 0, 0, 6, 0, 0, 0)  , width: 60 },
        { name:'Hour'       , format:'HH"h"'    , interval:  new Interval( 0, 0, 1, 0, 0, 0)  , width: 60, topWidth: 140, topFormat:'dd mmmm yyyy HH:MM' },
        { name:'10 minutes' , format:'MM"m"'    , interval:  new Interval( 0,10, 0, 0, 0, 0)  , width: 60 },
        { name:'1 minute'   , format:'MM"m"'    , interval:  new Interval( 0, 1, 0, 0, 0, 0)  , width: 60, topWidth: 140, topFormat:'dd mmmm yyyy HH:MM' },
        { name:'10 seconds' , format:'ss"s"'    , interval:  new Interval(10, 0, 0, 0, 0, 0)  , width: 30 },
        { name:'1 second'   , format:'ss"s"'    , interval:  new Interval( 1, 0, 0, 0, 0, 0)  , width: 30 }
    ],

    getLevelIndex: function(searchdetailization,width){
        width = width || 1;
        var targetLevel = _.find(RulerModel.levels, function(level){
            return level.interval.getSeconds()*1000 < searchdetailization/width;
        }) ;

        return typeof(targetLevel)!=='undefined' ? RulerModel.levels.indexOf(targetLevel) : RulerModel.levels.length-1;
    }
};












//Provider for records from mediaserver
function CameraRecordsProvider(cameras,mediaserver,$q,width) {

    this.cameras = cameras;
    this.mediaserver = mediaserver;
    this.$q = $q;
    this.chunksTree = null;
    this.width = width;
    var self = this;
    //1. request first detailization to get initial bounds

    this.requestInterval(0, self.now(), 0).then(function () {
        if(!self.chunksTree){
            return; //No chunks for this camera
        }
        // Depends on this interval - choose minimum interval, which contains all records and request deeper detailization
        var nextLevel = RulerModel.getLevelIndex(self.now() - self.chunksTree.start,self.width);
        console.log("record boundaries", self.chunksTree.title);
        self.requestInterval(self.chunksTree.start, self.now(), nextLevel);
    });

    //2. getCameraHistory
}

CameraRecordsProvider.prototype.now = function(){
    return (new Date()).getTime();
};

/**
 * Update active splice
 */
CameraRecordsProvider.prototype.updateSplice = function(){
    var ret = [];
    this.splice(ret,this.start, this.end, this.level);
    //console.log("send message to timeline for updating, or timeline should just get updated splice himself every redrawing")
};

var requestCounter = 0;
CameraRecordsProvider.prototype.requestInterval = function (start,end,level){
    var deferred = this.$q.defer();
    this.start = start;
    this.end = end;
    this.level = level;
    var detailization = RulerModel.levels[level].interval.getSeconds()*1000;

    var self = this;
    //1. Request records for interval
    // And do we need to request it?

    if(!self.lockRequests) {
        self.lockRequests = !this.chunksTree; // We may lock requests here
        this.mediaserver.getRecords('/', this.cameras[0], start, end, detailization)
            .then(function (data) {

                if(data.data.length == 0){
                    console.log("no chunks for this camera");

                    //Todo: find next chunk with a request.
                }
                for (var i = 0; i < data.data.length; i++) {
                    var endChunk = data.data[i][0] + data.data[i][1];
                    if (data.data[i][1] < 0) {
                        endChunk = (new Date()).getTime() + 100000;// some date in future
                    }
                    var addchunk = new Chunk(null, data.data[i][0], endChunk, level);


                    // console.log("read chunk",addchunk);
                    self.addChunk(addchunk);
                    self.lockRequests = false;//Unlock requests - we definitely have chunkstree here

                }

                //2. Splice cache for existing interval, store it as local cache
                self.updateSplice();

                console.log("resolved");
                deferred.resolve(self.chunksTree);
            }, function (error) {

                console.log("rejected");
                deferred.reject(error);
            });
    }else{
        deferred.reject("request in progress");
    }
    this.ready = deferred.promise;
    //3. return promise
    return this.ready;
};
/**
 * Request records for interval, add it to cache, update visibility splice
 *
 * @param start
 * @param end
 * @param level
 * @return Promise
 */
CameraRecordsProvider.prototype.setInterval = function (start,end,level){

    if(start instanceof Date)
    {
        start=start.getTime();
    }
    if(end instanceof Date)
    {
        end=end.getTime();
    }

    // Splice existing intervals and check, if we need an update from server
    //console.log("--------------------------------------------------");
    //console.log("setInterval",new Date(start),new Date(end),level);
    var result = [];
    var noNeedUpdate = this.splice(result,start,end,level);
    if(!noNeedUpdate){ // Request update if we have a tree
        if(this.chunksTree) {
            this.debug();
            this.requestInterval(start, end, level);
        }
    }
    // Return splice
    return result;
};


CameraRecordsProvider.prototype.debug = function(currentNode,level){
    if(!currentNode){
        console.log("Chunks tree:" + (this.chunksTree?"":"empty"));
    }
    level = level||0;
    currentNode = currentNode || this.chunksTree;

    if(currentNode) {
        console.log(Array(level + 1).join(" "), currentNode.title, currentNode.level,  currentNode.children.length);
        for (var i = 0; i < currentNode.children.length; i++) {
            this.debug(currentNode.children[i], level + 1);
        }
    }
};
/**
 * Add chunk to tree - find or create good position for it
 * @param chunk
 * @param parent - parent for recursive call
 */
CameraRecordsProvider.prototype.addChunk = function(chunk, parent){
    if(this.chunksTree === null || chunk.level === 0){
        this.chunksTree = chunk;
        return;
    }

    parent = parent || this.chunksTree;
    // Go through tree, find good place for chunk
    parent.updating = false;

    if(parent.children.length === 0 ){ // no children yet
        if(parent.level === chunk.level - 1){ //
            parent.children.push(chunk);
            return;
        }
        parent.children.push(new Chunk(parent,chunk.start,chunk.end,parent.level+1));
        this.addChunk(chunk,parent.children[0]);
    } else {
        for (var i = 0; i < parent.children.length; i++) {
            var currentChunk = parent.children[i];
            if (currentChunk.end < chunk.start) {
                continue;
            }
            if(currentChunk.level == chunk.level && currentChunk.end == chunk.end && currentChunk.start == chunk.start){
                return; //Skip the dublicate
            }

            if (currentChunk.level == chunk.level) {
                //we are on right position - place chunk before current
                parent.children.splice(i, 0, chunk);
            } else {
                //Here we should go deeper

                if(currentChunk.start <= chunk.start){ // We may have a miss here
                    this.addChunk(chunk, currentChunk);
                }else{
                    parent.children.splice(i, 0, new Chunk(parent, chunk.start, chunk.end, parent.level + 1));
                    this.addChunk(chunk, parent.children[i]);
                }

            }
            return;
        }

        if(parent.level === chunk.level - 1){ //
            parent.children.splice(i, 0, chunk);
        }else {
            parent.children.push(new Chunk(parent, chunk.start, chunk.end, parent.level + 1));
            this.addChunk(chunk, parent.children[i]);
        }


    }

    if(chunk.level === this.level && chunk.start >= this.start && chunk.end <= this.end){
        this.updateSplice();
    }
};

/**
 * Create splice, containing chunks with progressive detailization
 * (---------------------------------------------------------)
 * (-----)          (-------)     (--------------------------)
 * (--)  |          |  (--) |     (--------)    (---)   (----)
 * | ()  |          |  (--) |     (--) (-) |    | (-)   | | ()
 *
 * Example result:
 * (-----)          (-------)     (--) (-) |    (---)   (----)
 *
 * @param start
 * @param end
 * @param level
 * @param parent - parent for recursive call
 * @param result - heap for recursive call
 */
CameraRecordsProvider.prototype.splice = function(result, start, end, level, parent){

    parent = parent || this.chunksTree;

    var noNeedForUpdate = true;

    if(!parent){
        return false;
    }

    if (parent.children.length > 0) {
        for (var i = 0; i < parent.children.length; i++) {
            var currentChunk = parent.children[i];
            if (currentChunk.end <= start || currentChunk.start >= end || currentChunk.level === level) {
                currentChunk.expand = false;
                result.push(currentChunk);
            } else {
                currentChunk.expand = true;
                noNeedForUpdate = noNeedForUpdate && this.splice(result, start, end, level, currentChunk);
            }
        }
    } else {
        // Try to go deeper
        result.push(parent);
        if(!parent.updating ) { //prevent updating again
            console.log("we need to update chunk",parent, "to level",level);
            parent.updating = true;
            noNeedForUpdate = false; //We need to update
        }
    }

    return noNeedForUpdate;
};

CameraRecordsProvider.prototype.findNearestPosition = function (position,parent) {
    parent = parent || this.chunksTree;
    if (!parent) {
        return null;
    }

    if (parent.children.length > 0) {
        for (var i = 0; i < parent.children.length; i++) {
            var currentChunk = parent.children[i];
            if(currentChunk.start >= position ){
                console.log("start 1" , currentChunk);
                return currentChunk.start;
            }

            if(currentChunk.end > position ){
                //We may be in the actual chunk
                //We may have to go deeper
                return this.findNearestPosition(position,currentChunk);
            }
        }
    }

    if(parent.start >= position ){
        console.log("start 2" , parent);
        return parent.start;
    }

    if(parent.end < position){ //After the last chunk
        return null; //Go to live!
    }

    //Here we may want to request better detailization
    //console.warn("request deeper detailization", parent);

    return position; // Position is inside the chunk
};


/**
 * ShortCache - special collection for short cunks with best detailization for calculating position
 * @param start
 * @constructor
 */
function ShortCache(cameras,mediaserver,$q){

    this.mediaserver = mediaserver;
    this.cameras = cameras;

    this.start = 0; // Very start of video request
    this.played = 0;
    this.playedPosition = 0;
    this.lastRequestPosition = null;
    this.currentDetailization = [];
    this.checkPoints = {};
    this.updating = false; // flag to prevent updating twice
    this.lastPlayedPosition = 0; // Save the boundaries of uploaded cache
    this.lastPlayedDate = 0;

    this.requestInterval = 90 * 1000;//90 seconds to request
    this.updateInterval = 60 * 1000;//every 60 seconds update
    this.requestDetailization = 1;//the deepest detailization possible
    this.limitChunks = 10; // limit for number of chunks
    this.checkpointsFrequency = 1 * 60 * 1000;//Checkpoints - not often that 5 once in minutes
}
ShortCache.prototype.init = function(start){
    this.start = start;
    this.playedPosition = start;
    this.played = 0;
    this.lastRequestPosition = null;
    this.currentDetailization = [];
    this.checkPoints = {};
    this.updating = false;

    this.lastPlayedPosition = 0; // Save the boundaries of uploaded cache
    this.lastPlayedDate = start;

    this.update();
};

ShortCache.prototype.update = function(requestPosition,position){
    //Request from current moment to 1.5 minutes to future

    // Save them to this.currentDetailization
    if(this.updating && !requestPosition){ //Do not send request twice
        return;
    }
    console.log("update detailization",this.updating, requestPosition);

    requestPosition = requestPosition || this.playedPosition;

    this.updating = true;
    var self = this;
    this.lastRequestDate = requestPosition;
    this.lastRequestPosition = position || this.played;

    console.log("lastRequestPosition ",position, this.played, new Date(this.lastRequestDate));

    this.mediaserver.getRecords('/',
            this.cameras[0],
            requestPosition,
            (new Date()).getTime()+1000,
            this.requestDetailization,
            this.limitChunks).
        then(function(data){
            self.updating = false;
            if(data.data.length == 0){
                console.log("no chunks for this camera and interval");
            }
            if(data.data.length>0 && data.data[0][0] < requestPosition){ // Crop first chunk.
                data.data[0][1] -= requestPosition - data.data[0][0];
                data.data[0][0] = requestPosition;
            }
            self.currentDetailization = data.data;


            var lastCheckPointDate = self.lastRequestDate;
            var lastCheckPointPosition = self.lastRequestPosition;
            var lastSavedCheckpoint = 0; // First chunk will be forced to save

            for(var i=0; i<self.currentDetailization.length; i++){
                lastCheckPointPosition += self.currentDetailization[i][1];// Duration of chunks count
                lastCheckPointDate = self.currentDetailization[i][0] + self.currentDetailization[i][1];

                if( lastCheckPointDate - lastSavedCheckpoint > self.checkpointsFrequency) {
                    lastSavedCheckpoint = lastCheckPointDate;
                    self.checkPoints[lastCheckPointPosition] = lastCheckPointDate; // Too many checkpoints at this point
                }
            }
        });
};
// Check playing date - return videoposition if possible
ShortCache.prototype.checkPlayingDate = function(positionDate){
    if(positionDate < this.start){ //Check left boundaries
        console.log("too left!",new Date(positionDate), new Date(this.start), positionDate - this.start);
        return false; // Return negative value - outer code should request new videocache
    }

    if(positionDate > this.lastPlayedDate ){
        console.log("too right!",new Date(positionDate), new Date(this.lastPlayedDate), positionDate - this.lastPlayedDate);
        return false;
    }

    var lastPosition;
    for (var key in  this.checkPoints) {
        if ( this.checkPoints[key] > positionDate) {
            break;
        }
        lastPosition = key;
    }

    if(typeof(lastPosition)=='undefined'){// No checkpoints - go to live
        console.log("no checkpoints found - use the start point!");
        return positionDate - this.start;
    }

    return lastPosition + positionDate - this.checkPoints[key]; // Video should jump to this position
};

ShortCache.prototype.setPlayingPosition = function(position){
    // This function translate playing position (in millisecond) into actual date. Should be used while playing video only. Position must be in current buffered video
    this.played = position;
    this.playedPosition = 0;

    if(position < this.lastRequestPosition) { // Somewhere back on timeline
        var lastPosition;
        for (var key in  this.checkPoints) {
            if (key > position) {
                break;
            }
            lastPosition = key;
        }
        // lastPosition  is the nearest position in checkpoints
        // Estimate current playing position
        this.playedPosition = lastPosition + position - this.checkPoints[lastPosition];

        console.log("back on timeline");

        this.update(this.checkPoints[lastPosition], lastPosition);// Request detailization from that position and to the future - restore track
    }


    var intervalToEat = position - this.lastRequestPosition;
    this.playedPosition = this.lastRequestDate + intervalToEat;

    for(var i=0; i<this.currentDetailization.length; i++){
        intervalToEat -= this.currentDetailization[i][1];// Duration of chunks count
        this.playedPosition = this.currentDetailization[i][0] + this.currentDetailization[i][1] + intervalToEat;
        if(intervalToEat <= 0){
            break;
        }
    }

    var liveMode = false;
    if(i == this.currentDetailization.length){ // We have no good detailization for this moment - pretend to be playing live
        this.playedPosition = (new Date()).getTime();
        liveMode = true;
    }

    if(!liveMode && this.currentDetailization.length>0
        && (this.currentDetailization[this.currentDetailization.length - 1][1]
            + this.currentDetailization[this.currentDetailization.length - 1][0]
            < this.playedPosition + this.updateInterval)) { // It's time to update
        this.update();
    }

    //console.log("new played position",new Date(this.playedPosition),i,this.currentDetailization.length);


    if(position > this.lastPlayedPosition){
        this.lastPlayedPosition = position; // Save the boundaries of uploaded cache
        this.lastPlayedDate =  this.playedPosition; // Save the boundaries of uploaded cache
    }


    return this.playedPosition;
};


