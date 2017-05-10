'use strict';


//Record
function Chunk(boundaries,start,end,level,title,extension){
    this.start = start;
    this.end = end;
    this.level = level || 0;
    this.expand = true;

    var format = 'dd.mm.yyyy HH:MM:ss.l';
    this.title = (typeof(title) === 'undefined' || title === null) ? dateFormat(start,format) + ' - ' + dateFormat(end,format):title ;

    this.children = [];

    _.extend(this, extension);
}

Chunk.prototype.debug = function(){
    var format = 'dd.mm.yyyy HH:MM:ss.l';
    this.title = dateFormat(this.start,format) + ' - ' + dateFormat(this.end,format) ;

    console.log(new Array(this.level + 1).join(" "), this.title, this.level,  this.children.length);
};


function isDate(val){
    return val instanceof Date;
}
function NumberToDate(date){
    if(typeof(date)=='number' || typeof(date)=='Number'){
        date = Math.round(date);
        date = new Date(date);
    }
    if(!isDate(date)){
        console.error("non date " + (typeof date) + ": " + date );
        return null;
    }
    return date;
}

// Additional mini-library for declaring and using settings for ruler levels
function Interval (ms,seconds,minutes,hours,days,months,years){
    this.seconds = seconds;
    this.minutes = minutes;
    this.hours = hours;
    this.days = days;
    this.months = months;
    this.years = years;
    this.milliseconds = ms
};

Interval.prototype.addToDate = function(date, count){
    date = NumberToDate(date);

    if(typeof(count)==='undefined') {
        count = 1;
    }
    try {
        return new Date(date.getFullYear() + count * this.years,
                date.getMonth() + count * this.months,
                date.getDate() + count * this.days,
                date.getHours() + count * this.hours,
                date.getMinutes() + count * this.minutes,
                date.getSeconds() + count * this.seconds,
                date.getMilliseconds() + count * this.milliseconds);
    }catch(error){
        console.error("date problem" , date);
        throw error;
    }
};


/**
 * How many seconds are there in the interval.
 * Fucntion may return not exact value, it doesn't count leap years (difference doesn't matter in that case)
 * @returns {*}
 */
Interval.prototype.getMilliseconds = function(){
    var date1 = new Date(1971,11,1,0,0,0,0);//The first of december before the leap year. We need to count maximum interval (month -> 31 day, year -> 366 days)
    var date2 = this.addToDate(date1);
    return date2.getTime() - date1.getTime();
};

/**
 * Align to past. Can't work with intervals like "1 month and 3 days"
 * @param dateToAlign
 * @returns {Date}
 */
Interval.prototype.alignToPast = function(dateToAlign){
    var date = new Date(dateToAlign);

    if(this.milliseconds === 0){
        date.setMilliseconds(0);
    }else{
        date.setMilliseconds( Math.floor(date.getMilliseconds() / this.milliseconds) * this.milliseconds );
        return date;
    }

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
    date = NumberToDate(date);

    if(!date ){
        return false;
    }

    return this.alignToPast(date).getTime() === date.getTime();
};

Interval.prototype.alignToFuture = function(date){
    return this.alignToPast(this.addToDate(date));
};


/**
 * Special model for ruler levels. Stores all possible detailization level for the ruler
 * @type {{levels: *[], getLevelIndex: getLevelIndex}}
 */
var RulerModel = {

    /**
     * Presets for detailization levels
     * @type {{detailization: number}[]}
     */
    levels: [
        { name:'Age'        , interval:  new Interval(  0, 0, 0, 0, 0, 0,100), format:'yyyy' , marksW:15, smallW: 40, middleW: 400, width: 4000 , topW: 0, topFormat:'yyyy' }, // root
        { name:'Decade'     , interval:  new Interval(  0, 0, 0, 0, 0, 0, 10), format:'yyyy' , marksW:15, smallW: 40, middleW: 400, width: 9000 },
        {
            name:'Year', //Years
            format:'yyyy',//Format string for date
            interval:  new Interval(0,0,0,0,0,0,1),// Interval for marks
            marksW:15,   // width for marks without label
            smallW: 40,  // width for smallest label
            middleW: 120, // width for middle label
            width: 720, // width for label. We should choose minimal width in order to not intresect labels on timeline
            topW: 100, // minimal width for label above timeline
            topFormat:'yyyy'//Format string for label above timeline
        },
        { name:'6Months'   , interval:  new Interval(  0, 0, 0, 0, 0, 6, 0), format:'mmmm' , marksW:15, smallW: 60, middleW: 360, width: 3600 },
        { name:'3Months'   , interval:  new Interval(  0, 0, 0, 0, 0, 3, 0), format:'mmmm' , marksW:15, smallW: 60, middleW: 1800, width: 9000 },
        { name:'Month'      , interval:  new Interval(  0, 0, 0, 0, 0, 1, 0), format:'mmmm' , marksW:15, smallW: 60, middleW: 100000,   width: 600 , topW: 170, topFormat:'mmmm yyyy'},
        { name:'Day'        , interval:  new Interval(  0, 0, 0, 0, 1, 0, 0), format:'dd'   , marksW:5,  smallW: 20, middleW: 100,      width: 200 , topW: 170, topFormat:'d mmmm yyyy' ,contained:1},
        { name:'12h'   , interval:  new Interval(  0, 0, 0,12, 0, 0, 0), format:'HH:MM', marksW:15, smallW: 50, middleW: 200,      width: 1200},
        { name:'6h'    , interval:  new Interval(  0, 0, 0, 6, 0, 0, 0), format:'HH:MM', marksW:15, smallW: 50, middleW: 600,      width: 1800 },
        { name:'3h'    , interval:  new Interval(  0, 0, 0, 3, 0, 0, 0), format:'HH:MM', marksW:15, smallW: 50, middleW: 300,      width: 900 },
        { name:'1h'       , interval:  new Interval(  0, 0, 0, 1, 0, 0, 0), format:'HH:MM', marksW:15, smallW: 50, middleW: 100,      width: 300 },
        { name:'30m' , interval:  new Interval(  0, 0,30, 0, 0, 0, 0), format:'HH:MM', marksW:15, smallW: 50, middleW: 300,      width: 1500 },
        { name:'10m' , interval:  new Interval(  0, 0,10, 0, 0, 0, 0), format:'HH:MM', marksW:15, smallW: 50, middleW: 500,      width: 1000 },
        { name:'5m'  , interval:  new Interval(  0, 0, 5, 0, 0, 0, 0), format:'HH:MM', marksW:15, smallW: 50, middleW: 250,      width: 500 },
        { name:'1m'   , interval:  new Interval(  0, 0, 1, 0, 0, 0, 0), format:'HH:MM', marksW:15, smallW: 50, middleW: 100,      width: 300 , topW: 170, topFormat:'d mmmm yyyy HH:MM'},
        { name:'30s' , interval:  new Interval(  0,30, 0, 0, 0, 0, 0), format:'ss"s"', marksW:15, smallW: 50, middleW: 300,      width: 1500},
        { name:'10s' , interval:  new Interval(  0,10, 0, 0, 0, 0, 0), format:'ss"s"', marksW:15, smallW: 50, middleW: 500,      width: 1000 },
        { name:'5s'  , interval:  new Interval(  0, 5, 0, 0, 0, 0, 0), format:'ss"s"', marksW:15, smallW: 50, middleW: 250,      width: 100000 },
        { name:'1s'   , interval:  new Interval(  0, 1, 0, 0, 0, 0, 0), format:'ss"s"', marksW:15, smallW: 50, middleW: 100000,   width: 100000 , topW: 200, topFormat:'d mmmm yyyy HH:MM:ss'}
        //{ name:'500ms'     , interval:  new Interval(500, 0, 0, 0, 0, 0, 0), format:'l"ms"', marksW:15, smallW: 50, middleW: 250,      width: 100000},
        //{ name:'100ms'     , interval:  new Interval(100, 0, 0, 0, 0, 0, 0), format:'l"ms"', marksW:15, smallW: 50, middleW: 100000,   width: 100000 }
    ],

    getLevelIndex: function(searchdetailization,width){
        width = width || 1;
        var targetLevel = _.find(RulerModel.levels, function(level){
            return level.interval.getMilliseconds() < searchdetailization/width;
        }) ;

        return typeof(targetLevel)!=='undefined' ? this.levels.indexOf(targetLevel) : this.levels.length-1;
    },


    findBestLevelIndex:function(date, maxLevelIndex){
        if(maxLevelIndex) {
            var maxLevel = Math.min(maxLevelIndex - 1, this.levels.length - 1);

            for (var i = maxLevel; i >= 0; i--) {
                if (!this.levels[i].interval.checkDate(date)) {
                    return i + 1;
                }
            }
            return i;
        }

        for (var i = 0; i < this.levels.length; i++) {
            if (this.levels[i].interval.checkDate(date)) {
                return i;
            }
        }
        return this.levels.length-1;

    },
    findBestLevel:function(date){
        return this.levels[this.findBestLevelIndex(date)];
    }
};

//Provider for records from mediaserver
function CameraRecordsProvider(cameras,mediaserver,$q,width,timeCorrection) {

    this.cameras = cameras;
    this.mediaserver = mediaserver;
    this.$q = $q;
    this.chunksTree = null;
    this.requestedCache = [];
    this.timeCorrection = timeCorrection || 0;
    var self = this;
    //1. request first detailization to get initial bounds

    var archiveReadyDefer = $q.defer();
    this.archiveReadyPromise = archiveReadyDefer.promise;
    this.lastRequested = self.now();
    this.requestInterval(0, self.now() + 10000, 0).then(function () {
        archiveReadyDefer.resolve(!!self.chunksTree);
        if(!self.chunksTree){
            return; //No chunks for this camera
        }

        // Depends on this interval - choose minimum interval, which contains all records and request deeper detailization
        var nextLevel = RulerModel.getLevelIndex(self.now() - self.chunksTree.start,width);
        if(nextLevel<RulerModel.levels.length-1) {
            self.requestInterval(self.chunksTree.start, self.now(), nextLevel + 1);
        }
    });

    //2. getCameraHistory
}

CameraRecordsProvider.prototype.now = function(){
    return (new Date()).getTime();
};

CameraRecordsProvider.prototype.cacheRequestedInterval = function (start,end,level){
    for(var i=0;i<level+1;i++){
        if(i >= this.requestedCache.length){
            this.requestedCache.push([{start:start,end:end}]); //Add new cache level
            continue;
        }

        //Find good position to cache new requested interval
        for(var j=0;j<this.requestedCache[i].length;j++){
            if(this.requestedCache[i][j].end < start){
                continue;
            }

            if(this.requestedCache[i][j].start > end){ // no intersection - just add
                this.requestedCache[i].splice(j,0,{start:start,end:end});
                break;
            }

            this.requestedCache[i][j].start = Math.min(start,this.requestedCache[i][j].start);
            this.requestedCache[i][j].end = Math.max(end,this.requestedCache[i][j].end);
            break;
        }
    }
};
CameraRecordsProvider.prototype.getBlindSpotsInCache = function(start,end,level){
    if(typeof(this.requestedCache[level])=='undefined'){
        return [{start:start,end:end}]; // One large blind spot
    }
    var result = [];

    var levelCache = this.requestedCache[level];
    for(var i = 0;i < levelCache.length;i++) {

        if(start < levelCache[i].start){
            result.push ({start:start,end:levelCache[i].start});
        }

        start = Math.max(start,levelCache[i].end);//Move start

        if(end < levelCache[i].end){
            break;
        }
    }
    return result;
};

CameraRecordsProvider.prototype.checkRequestedIntervalCache = function (start,end,level) {
    if(typeof(this.requestedCache[level])=='undefined'){
        return false;
    }
    var levelCache = this.requestedCache[level];

    for(var i=0;i < levelCache.length;i++) {
        if(start < levelCache[i].start){
            return false;
        }

        start = Math.max(start,levelCache[i].end);//Move start

        if(end < levelCache[i].end){
            return true;
        }
    }
    return end <= start;
};

CameraRecordsProvider.prototype.updateLastMinute = function(lastMinuteDuration, level){
    var now = this.now();

    if(now - this.lastRequested > lastMinuteDuration/2) {
        this.requestInterval(now - lastMinuteDuration, now + 10000,level);
        this.lastRequested = now;
    }
};

CameraRecordsProvider.prototype.abort = function (reason){
    if(this.currentRequest) {
        this.currentRequest.abort(reason);
        this.currentRequest = null;
    }
};

CameraRecordsProvider.prototype.requestInterval = function (start,end,level){
    var deferred = this.$q.defer();
    //this.start = start;
    //this.end = end;
    this.level = level;
    var levelData = RulerModel.levels[level];
    var detailization = levelData.interval.getMilliseconds();

    var self = this;
    //1. Request records for interval
    // And do we need to request it?

    if(self.currentRequest){
        return;
    }
    self.currentRequest = this.mediaserver.getRecords('/', this.cameras[0], Math.max(start - this.timeCorrection,0), end - this.timeCorrection, detailization, null, levelData.name);

    self.currentRequest.then(function (data) {

            self.currentRequest = null;//Unlock requests - we definitely have chunkstree here
            var chunks = data.data.reply;
            //if(chunks.length == 0){} // No chunks for this camera

            _.forEach(chunks,function(chunk){
                chunk.durationMs = parseInt(chunk.durationMs);
                chunk.startTimeMs = parseInt(chunk.startTimeMs) + self.timeCorrection;
            });

            var chunksToIterate = chunks.length;
            //If level == 0 - we want only first chunk
            if(!level && chunks.length>0){ // Special hack
                chunks[0].durationMs = -1; // Make it 'live'
                chunksToIterate = 1;
            }
            for (var i = 0; i < chunksToIterate; i++) {
                var endChunk = chunks[i].startTimeMs + chunks[i].durationMs;
                if (chunks[i].durationMs < 0) {
                    endChunk = (new Date()).getTime();// current moment
                }
                var addchunk = new Chunk(null, chunks[i].startTimeMs, endChunk, level);
                self.addChunk(addchunk, null);
            }

            self.cacheRequestedInterval(start,end,level);

            deferred.resolve(self.chunksTree);

        }, function (error) {
            deferred.reject(error);
        });

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
 * @return Array
 */
CameraRecordsProvider.prototype.getIntervalRecords = function (start,end,level,debugLevel){

    if(start instanceof Date)
    {
        start = start.getTime();
    }
    if(end instanceof Date)
    {
        end = end.getTime();
    }

    // Splice existing intervals and check, if we need an update from server
    var result = [];
    this.selectRecords(result,start,end,level,null,debugLevel);

    /*this.logcounter = this.logcounter||0;
    this.logcounter ++;
    if(this.logcounter % 1000 === 0) {
        log("splice: ============================================================");
        for (var i = 0; i < result.length; i++) {
            result[i].debug();
        }
        this.debug();
    }*/

    var needUpdate = !debugLevel && !this.checkRequestedIntervalCache(start,end,level);
    if(needUpdate){ // Request update
        this.requestInterval(start, end, level);
    }

    // Return splice - as is
    return result;
};

CameraRecordsProvider.prototype.debug = function(currentNode,depth){
    if(!currentNode){
        console.log("Chunks tree:" + (this.chunksTree?"":"empty"));
    }
    if(typeof(depth) == "undefined"){
        depth = RulerModel.levels.length - 1;
    }
    currentNode = currentNode || this.chunksTree;

    if(currentNode) {
        currentNode.debug();
        if(depth>0) {
            for (var i = 0; i < currentNode.children.length; i++) {
                this.debug(currentNode.children[i], depth - 1);
            }
        }
    }
};

CameraRecordsProvider.prototype.debugSplice = function(events,level){
    console.log("Events:", level, events.length);
    for(var i=0;i<events.length;i++){
        events[i].debug();
    }
    console.log("/ Events:", level, events.length);
};
/**
 * Add chunk to tree - find or create good position for it
 * @param chunk
 * @param parent - parent for recursive callрп
 */
CameraRecordsProvider.prototype.addChunk = function(chunk, parent){
    if(this.chunksTree === null || chunk.level === 0){
        this.chunksTree = chunk;
        return;
    }

    parent = parent || this.chunksTree;

    if(chunk.level <= parent.level){
        console.error("something wrong happened",chunk,parent);
        return;
    }

    if(parent.end < chunk.end){
        parent.end = chunk.end;
    }

    // Go through tree, find good place for chunk
    if(parent.children.length === 0 ){ // no children yet
        if(parent.level === chunk.level - 1){ //
            parent.children.push(chunk);
            return;
        }
        parent.children.push(new Chunk(parent,chunk.start,chunk.end,parent.level+1));
        this.addChunk(chunk,parent.children[0]);
        return;
    }

    for (var i = 0; i < parent.children.length; i++) {
        var iteratingChunk = parent.children[i];

        if (iteratingChunk.end < chunk.start) { //no intersection - no way we need this chunk now
            continue;
        }

        if (iteratingChunk.end >= chunk.end && iteratingChunk.start <= chunk.start) { // Contained chunk
            if (iteratingChunk.level != chunk.level) { // Add inside ot ignore
                this.addChunk(chunk, iteratingChunk);
            }
            return;
        }

        if (iteratingChunk.start < chunk.end) { // Intersection here!
            // Cut chunk into pieces!
            if(chunk.start < iteratingChunk.start){
                iteratingChunk.start = chunk.start;
            }

            if(chunk.end > iteratingChunk.end){
                if(!parent.children[i+1] || parent.children[i+1].start >= chunk.end){
                    iteratingChunk.end = chunk.end;
                }else {
                    // TODO: WTF is going here?
                    var rightChunk = new Chunk(null, iteratingChunk.end + 1, chunk.end, chunk.level);
                    chunk.end = iteratingChunk.end;
                    this.addChunk(rightChunk, parent);
                }
            }

            if (iteratingChunk.level != chunk.level) { // Add inside ot ignore
                this.addChunk(chunk, iteratingChunk);
            }
            return;
        }

        // No intersection - add here
        if (parent.level == chunk.level - 1) {
            parent.children.splice(i, 0, chunk);//Just add chunk here and return
        } else {
            parent.children.splice(i, 0, new Chunk(parent, chunk.start, chunk.end, parent.level + 1)); //Create new children and go deeper
            this.addChunk(chunk, parent.children[i]);
        }
        return;
    }

    if (parent.level == chunk.level - 1) {
        parent.children.push(chunk);
    }else {
        //Add new last chunk and go deeper
        parent.children.push(new Chunk(parent, chunk.start, chunk.end, parent.level + 1)); //Create new chunk
        this.addChunk(chunk, parent.children[i]);
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
CameraRecordsProvider.prototype.selectRecords = function(result, start, end, level, parent, debugLevel){
    parent = parent || this.chunksTree;

    if(!parent){
        return;
    }

    if(parent.end <= start || parent.start >= end ){ // Not intresected
        return;
    }

    if(parent.level == level){
        result.push(parent); // Good chunk!
        return;
    }

    if(parent.children.length == 0){
        //if(!debugLevel) {
            result.push(parent); // Bad chunk, but no choice
            return;
        //}
    }

    // Iterate children:
    for (var i = 0; i < parent.children.length; i++) {
        this.selectRecords(result, start, end, level, parent.children[i]);
    }

    // iterate blind spots:
    var blindSpots = this.getBlindSpotsInCache(parent.start, parent.end, parent.level + 1);
    for (i = 0; i < blindSpots.length; i++) {
        var spot = blindSpots[i];
        if (spot.start >= start || spot.end <= end) {
            result.push(spot);
        }
    }
};


/**
 * ShortCache - special collection for short chunks with best detailization for calculating playing position and date
 * @constructor
 */
function ShortCache(cameras,mediaserver,$q,timeCorrection){

    this.$q = $q;

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
    this.limitChunks = 100; // limit for number of chunks
    this.checkpointsFrequency = 60 * 1000;//Checkpoints - not often that once in a minute

    this.timeCorrection = timeCorrection || 0;
}
ShortCache.prototype.init = function(start, timeCorrection, isPlaying){
    this.liveMode = false;
    if(!start){
        this.liveMode = true;
        start = (new Date()).getTime();
    }
    this.start = start;
    this.playedPosition = start;
    this.played = 0;
    this.lastRequestPosition = null;
    this.currentDetailization = [];
    this.checkPoints = {};
    this.updating = false;

    this.lastPlayedPosition = 0; // Save the boundaries of uploaded cache
    this.lastPlayedDate = 0;
    this.timeCorrection = timeCorrection;
    this.playing = typeof(isPlaying) != "undefined" ? isPlaying : true;

    this.update();
};

ShortCache.prototype.abort = function(reason){
    if(this.currentRequest) {
        this.currentRequest.abort(reason);
        this.currentRequest = null;
    }
};

ShortCache.prototype.update = function(requestPosition,position){
    //Request from current moment to 1.5 minutes to future

    // Save them to this.currentDetailization
    if(this.updating && !requestPosition){ //Do not send request twice
        return;
    }

    requestPosition = Math.round(requestPosition) || Math.round(this.playedPosition);

    this.updating = true;
    var self = this;
    this.lastRequestDate = requestPosition;
    this.lastRequestPosition = position || this.played;

    this.abort("update again");
    // Get next {{limitChunks}} chunks
    //console.log("Start Time: %s\tEnd Time: %s",Math.max(requestPosition-this.timeCorrection,0),(new Date()).getTime()+100000 - this.timeCorrection);
    this.currentRequest = this.mediaserver.getRecords('/',
            this.cameras[0],
            Math.max(requestPosition - this.timeCorrection,0),
            (new Date()).getTime() + 100000 - this.timeCorrection,
            this.requestDetailization,
            this.limitChunks);

    this.currentRequest.then(function(data){
            self.updating = false;

            var chunks = data.data.reply;

            _.forEach(chunks,function(chunk){
                chunk.durationMs = parseInt(chunk.durationMs);
                chunk.startTimeMs = parseInt(chunk.startTimeMs) + self.timeCorrection;

                if(chunk.durationMs == -1){
                    chunk.durationMs = (new Date()).getTime() - chunk.startTimeMs;//in future
                }
            });

            //if(chunks.length == 0){ } // no chunks for this camera and interval


            if(chunks.length > 0 && chunks[0].startTimeMs < requestPosition){ // Crop first chunk.
                chunks[0].durationMs += chunks[0].startTimeMs - requestPosition ;
                chunks[0].startTimeMs = requestPosition;
            }
            self.currentDetailization = chunks;

            var lastCheckPointDate = self.lastRequestDate;
            var lastCheckPointPosition = self.lastRequestPosition;
            var lastSavedCheckpoint = 0; // First chunk will be forced to save

            for(var i=0; i<self.currentDetailization.length; i++){
                lastCheckPointPosition += self.currentDetailization[i].durationMs;// Duration of chunks count
                lastCheckPointDate = self.currentDetailization[i].startTimeMs + self.currentDetailization[i].durationMs;

                if( lastCheckPointDate - lastSavedCheckpoint > self.checkpointsFrequency) {
                    lastSavedCheckpoint = lastCheckPointDate;
                    self.checkPoints[lastCheckPointPosition] = lastCheckPointDate; // Too many checkpoints at this point
                }
            }

            self.setPlayingPosition(self.played);
        });
};

// Check playing date - return videoposition if possible
ShortCache.prototype.checkPlayingDate = function(positionDate){
    if(positionDate < this.start){ //Check left boundaries
        return false; // Return negative value - outer code should request new videocache
    }

    if(positionDate > this.lastPlayedDate ){
        return false;
    }

    var lastPosition;
    for (var key in  this.checkPoints) {
        if(this.checkPoints.hasOwnProperty(key)) {
            if (this.checkPoints[key] > positionDate) {
                break;
            }
            lastPosition = key;
        }
    }

    if(typeof(lastPosition)=='undefined'){// No checkpoints - go to live
        return positionDate - this.start;
    }

    return lastPosition + positionDate - this.checkPoints[key]; // Video should jump to this position
};


ShortCache.prototype.setPlayingPosition = function(position){
    // This function translate playing position (in millisecond) into actual date. Should be used while playing video only. Position must be in current buffered video

    var oldPosition =  this.playedPosition;

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
        this.chunkPosition = 0;
        this.playedPosition = lastPosition + position - this.checkPoints[lastPosition];

        this.update(this.checkPoints[lastPosition], lastPosition);// Request detailization from that position and to the future - restore track
    }


    var intervalToEat = position - this.lastRequestPosition;
    this.playedPosition = this.lastRequestDate + intervalToEat;
    for(var i= 0; i<this.currentDetailization.length; i++){
        intervalToEat -= this.currentDetailization[i].durationMs;// Duration of chunks count
        this.playedPosition = this.currentDetailization[i].startTimeMs + this.currentDetailization[i].durationMs + intervalToEat;
        if(intervalToEat <= 0){
            break;
        }
    }

    //this.liveMode = false;
    if(i == this.currentDetailization.length){ // We have no good detailization for this moment - pretend to be playing live

        if(!this.updating){
            this.playedPosition = (new Date()).getTime();
            this.liveMode = true;
        }
    }

    if(!this.liveMode && this.currentDetailization.length>0
        && (this.currentDetailization[this.currentDetailization.length - 1].durationMs
            + this.currentDetailization[this.currentDetailization.length - 1].startTimeMs
            < Math.round(this.playedPosition) + this.updateInterval)) { // It's time to update

        // TODO: update live detailization?

        this.update();
    }

    if(position > this.lastPlayedPosition){
        this.lastPlayedPosition = position; // Save the boundaries of uploaded cache
        this.lastPlayedDate =  this.playedPosition; // Save the boundaries of uploaded cache
    }

    if(oldPosition > this.playedPosition && Config.allowDebugMode){
        console.error("Position jumped back! ms:" , oldPosition - this.playedPosition);
    }
    
    return this.playedPosition;
};






function ScaleManager (minMsPerPixel, maxMsPerPixel, defaultIntervalInMS, initialWidth, stickToLiveMs, zoomAccuracyMs, lastMinuteInterval, minPixelsPerLevel, useServerTime, $q){
    this.absMaxMsPerPixel = maxMsPerPixel;
    this.minMsPerPixel = minMsPerPixel;
    this.stickToLiveMs = stickToLiveMs;
    this.zoomAccuracyMs = zoomAccuracyMs;
    this.minPixelsPerLevel = minPixelsPerLevel;
    this.lastMinuteInterval  = lastMinuteInterval;
    this.useServerTime = useServerTime;
    this.$q = $q;

    this.levels = {
        top:  {index:0,level:RulerModel.levels[0]},
        labels:     {index:0,level:RulerModel.levels[0]},
        middle:     {index:0,level:RulerModel.levels[0]},
        small:      {index:0,level:RulerModel.levels[0]},
        marks:      {index:0,level:RulerModel.levels[0]},
        events:     {index:0,level:RulerModel.levels[0]}
    };

// Setup total boundaries
    this.viewportWidth = initialWidth;
    this.end = (new Date()).getTime();
    this.start = this.end - defaultIntervalInMS;
    this.updateTotalInterval();

// Setup default scroll window
    this.msPerPixel = this.maxMsPerPixel;
    this.anchorPoint = 1;
    this.anchorDate = this.end;
    this.updateCurrentInterval();

    this.timeZoneOffset = 0;
    this.latency = 0;
}

ScaleManager.prototype.updateTotalInterval = function(){
    //Calculate maxmxPerPixel
    this.maxMsPerPixel = (this.end - this.start) / this.viewportWidth;
    this.msPerPixel = Math.min(this.msPerPixel,this.maxMsPerPixel);
};
ScaleManager.prototype.setViewportWidth = function(width){ // For initialization and window resize
    this.viewportWidth = width;
    this.updateTotalInterval();
    this.updateCurrentInterval();
};
ScaleManager.prototype.setStart = function(start){// Update the begining end of the timeline. Live mode must be supported here
    this.start = this.serverTime(start);
    this.updateTotalInterval();
};
ScaleManager.prototype.setEnd = function(end){ // Update right end of the timeline. Live mode must be supported here
    var needZoomOut = !this.checkZoomOut();
    this.end = this.serverTime(end);
    this.updateTotalInterval();
    if(needZoomOut){
        this.zoom(1);
    }
};

ScaleManager.prototype.bound = function (min,val,max){
    if(min > max){
    //    console.warn("screwed bound");
        // var i = max;
        // max = min;
        // min = i;
    }
    val = Math.max(val,min);
    return Math.min(val,max);
};


ScaleManager.prototype.updateCurrentInterval = function(){
    //Calculate visibleEnd and visibleStart
    this.msPerPixel = this.bound(this.minMsPerPixel, this.msPerPixel, this.maxMsPerPixel);
    this.anchorPoint = this.bound(0, this.anchorPoint, 1);
    this.anchorDate = this.bound(this.start, Math.round(this.anchorDate), this.end);


    this.visibleStart = Math.round(this.anchorDate - this.msPerPixel * this.viewportWidth * this.anchorPoint);
    this.visibleEnd = Math.round(this.anchorDate + this.msPerPixel  * this.viewportWidth * (1 - this.anchorPoint));


    if(this.visibleStart < this.start){
        //Move end
        this.visibleEnd += this.start - this.visibleStart;
        this.visibleStart = this.start;
    }

    if(this.visibleEnd > this.end){
        //Move end
        this.visibleStart += this.end - this.visibleEnd;
        this.visibleEnd = this.end;
    }

    if(this.visibleStart > this.visibleEnd){
        console.error ("wtf! visibleStart > visibleEnd ?",new Date(this.visibleStart),new Date(this.visibleEnd));
    }

    // Adjust here with msPerPixel again!
    this.updateLevels();
};


ScaleManager.prototype.stopWatching = function(){
    this.watchPlayingPosition = false;
    this.wasForcedToStopWatchPlaying = true;
};

ScaleManager.prototype.releaseWatching = function(){
    this.wasForcedToStopWatchPlaying = false;
};

ScaleManager.prototype.watchPlaying = function(date, liveMode){
    this.watchPlayingPosition = true;
    this.wasForcedToStopWatchPlaying = false;

    if(!date){
        return;
    }
    var targetPoint = this.dateToScreenCoordinate(date) / this.viewportWidth;
    if(targetPoint > 1){
        targetPoint = 0.5;
    }
    if(liveMode){
        targetPoint = 1;
    }
    this.setAnchorDateAndPoint(date, targetPoint);
};

ScaleManager.prototype.checkWatchPlaying = function(date,liveMode){
    var targetPoint = this.dateToScreenCoordinate(date)/this.viewportWidth;

    if(this.anchorDate == date){
        this.watchPlaying(date);
    }

    if(0 <= targetPoint && targetPoint <= 1 ){ 
        this.watchPlaying(date); // Let's watch
    }

    if(liveMode && targetPoint > 1 && this.end - this.visibleEnd < this.stickToLiveMs ){
        this.watchPlaying(date, liveMode);
    }
};

ScaleManager.prototype.tryToSetLiveDate = function(playing, liveMode, end){
    this.playedPosition = playing;
    this.liveMode = liveMode;

    if(this.anchorDate == playing){
        return;
    }

    if(playing > this.end && liveMode){
        this.setEnd(playing);
    }else if(end > this.end){
        this.setEnd(end);
    }

    if(!this.wasForcedToStopWatchPlaying && !this.watchPlayingPosition){
        this.checkWatchPlaying(playing,liveMode);
    }
    if(this.watchPlayingPosition){
        this.setAnchorDateAndPoint(playing, liveMode ? 1:this.anchorPoint);
    }
};



ScaleManager.prototype.tryToRestoreAnchorDate = function(date){
    var targetPoint = this.dateToScreenCoordinate(date)/this.viewportWidth;
    if(0 <= targetPoint && targetPoint <= 1){
        //this.setAnchorDateAndPoint(date, targetPoint);
        this.anchorDate = date;
        this.anchorPoint = targetPoint;
    }
};

ScaleManager.prototype.setAnchorCoordinate = function(coordinate){ // Set anchor date
    this.setAnchorDateAndPoint(this.screenCoordinateToDate(coordinate),coordinate / this.viewportWidth);
};

ScaleManager.prototype.setAnchorDateAndPoint = function(date,point){ // Set anchor date
    this.anchorDate = date;
    if(typeof(point)!="undefined") {
        //console.log('anchorPoint', point);
        this.anchorPoint = point;
    }
    this.updateCurrentInterval();
};


ScaleManager.prototype.fullZoomOutValue = function(){
    return this.msToZoom(this.maxMsPerPixel);
};
ScaleManager.prototype.fullZoomInValue = function(){
    return this.msToZoom(this.minMsPerPixel);
};

ScaleManager.prototype.boundZoom = function(zoomTarget){
    return this.bound(this.fullZoomInValue(),zoomTarget,this.fullZoomOutValue());
};


ScaleManager.prototype.calcLevels = function(msPerPixel) {
    var zerolevel = RulerModel.levels[0];
    var levels = {
        top:{index:0,level:zerolevel},
        labels:{index:0,level:zerolevel},
        middle:{index:0,level:zerolevel},
        small:{index:0,level:zerolevel},
        marks:{index:0,level:zerolevel},
        events:{index:0,level:zerolevel}
    };
    for (var i = 0; i < RulerModel.levels.length; i++) {
        var level = RulerModel.levels[i];

        var pixelsPerLevel = (level.interval.getMilliseconds() / msPerPixel );

        if (typeof(level.topW) !== 'undefined' &&
            pixelsPerLevel >= level.topW) {
            levels.top = {index:i,level:level};
        }

        if (pixelsPerLevel >= level.width) {
            levels.labels = {index:i,level:level};
        }

        if (pixelsPerLevel >= level.middleW) {
            levels.middle = {index:i,level:level};
        }

        if (pixelsPerLevel >= level.smallW) {
            levels.small = {index:i,level:level};
        }

        if (pixelsPerLevel >= level.marksW) {
            levels.marks = {index:i,level:level};
        }

        levels.events = {index:i,level:level};

        if (pixelsPerLevel <= this.minPixelsPerLevel) {
            // minMsPerPixel
            break;
        }
    }
    /*if(levels.top.index == levels.labels.index && levels.top.index > 0){
        do{
            levels.top.index --;
            levels.top.level = RulerModel.levels[levels.top.index];
        }while(!levels.top.level.topW && levels.top.index);
    }*/
    return levels;
};
ScaleManager.prototype.updateLevels = function() {
    //3. Select actual level
    this.levels = this.calcLevels(this.msPerPixel);
};

ScaleManager.prototype.alignStart = function(level){ // Align start by the grid using level
    return level.interval.alignToPast(this.visibleStart);
};
ScaleManager.prototype.alignEnd = function(level){ // Align end by the grid using level
    return level.interval.alignToFuture(this.visibleEnd);
};

ScaleManager.prototype.coordinateToDate = function(coordinate){
    return Math.round(this.start + coordinate * this.msPerPixel);
};
ScaleManager.prototype.dateToCoordinate = function(date){
    return  (date - this.start) / this.msPerPixel;
};

ScaleManager.prototype.dateToScreenCoordinate = function(date){
    return this.dateToCoordinate(date) - this.dateToCoordinate(this.visibleStart);
};
ScaleManager.prototype.screenCoordinateToDate = function(coordinate){
    return this.coordinateToDate(coordinate + this.dateToCoordinate(this.visibleStart));
};

// Some function for scroll support
// Actually, scroll is based on some AnchorDate and AnchorPosition (on the screen 0 - 1). And anchor may be "live", which means live-scrolling
// So there is absolute scroll position in percents
// If we move scroll right or left - we are loosing old anchor and setting new.
// If we are zooming - we are zooming around anchor.

ScaleManager.prototype.getRelativeCenter = function(){
    return ((this.visibleStart + this.visibleEnd) / 2 - this.start) / (this.end - this.start);
};
ScaleManager.prototype.getRelativeWidth = function(){
    return this.msPerPixel * this.viewportWidth / (this.end - this.start);
};



ScaleManager.prototype.canScroll = function(left){
    if(left){
        return this.visibleStart != this.start;
    }

    return this.visibleEnd != this.end
        && !(this.watchPlayingPosition && this.liveMode);
};

ScaleManager.prototype.scroll = function(value){
    if(typeof (value) == "undefined"){
        return this.getRelativeCenter();
    }
    value = this.bound(0,value,1);
    //scroll right or left by relative value - move anchor

    var achcorDate = this.anchorDate; //Save anchorDate
    this.setAnchorDateAndPoint(this.start + value * (this.end-this.start),0.5); //Move viewport
    this.tryToRestoreAnchorDate(achcorDate); //Try to restore anchorDate
};

ScaleManager.prototype.getScrollByPixelsTarget = function(pixels){
    return this.bound(0,this.scroll() + pixels * this.getRelativeWidth() / this.viewportWidth,1);
};

ScaleManager.prototype.scrollByPixels = function(pixels){
    //scroll right or left by relative value - move anchor date
    this.scroll(this.getScrollByPixelsTarget(pixels));
};


/*
 * Details about zoom logic:
 * Main internal parameters, responsible for actual zoom is msPerPixel - number of miliseconds per one pixel on the screen, it changes from minMsPerPixel to absMaxMsPerPixel
 * Both extreme values are unreachable: we can't zoom out to years if we have archive only for two months. And we can't zoom to 0 ms/pixel (infinite width of the timeline)
 *
 * So there are actual minimum and maximum values for msPerPixel.
 *
 * Outer code operates with zoom as relative value from 0 to 1 and we have to translate that value to msPerPixel with some function.
 *
 * Our changing zoom is proportional to actual msPerPixel
 *
 * So, finally we have differential equation:
 *
 * X - msPerPixel
 * z - zoom
 *
 * Xmin = minMsPerPixel
 * Xmax = maxMsPerPixel
 *
 *
 * x'(z)*dz = k*x(z)
 * x(0) = Xmin
 * x(1) = Xmax
 * z € [0,1]
 *
 * x(z) = C*e^(k*z)
 * x(0) = Xmin = C
 * x(1) = Xmax = Xmin * e^k
 * k = ln(Xmax/Xmin)
 *
 * x(z) = Xmin * e^( ln(Xmax/Xmin) * z) = Xmin * (Xmax/Xmin) ^ z
  *
  * Wow!
  *
  * z(x) = ln(x/Xmin) / ln (Xmax/Xmin)
  *
  * Rejoice!
  *
 **/

ScaleManager.prototype.zoomToMs = function(zoom){
    // x(z) = Xmin * e^( ln(Xmax/Xmin) * z) = Xmin * (Xmax/Xmin) ^ z
    var msPerPixel = this.minMsPerPixel * Math.pow(this.absMaxMsPerPixel / this.minMsPerPixel,zoom);
    return this.bound(this.minMsPerPixel, msPerPixel, this.maxMsPerPixel);
};
ScaleManager.prototype.msToZoom = function(ms){
    // z(x) = ln(x/Xmin) / ln (Xmax/Xmin)
    var zoom = Math.log(ms/this.minMsPerPixel) / Math.log(this.absMaxMsPerPixel / this.minMsPerPixel);
    return zoom;
};


ScaleManager.prototype.targetLevels = function(zoomTarget){
    zoomTarget = this.boundZoom(zoomTarget);
    var msPerPixel = this.zoomToMs(zoomTarget);
    return this.calcLevels(msPerPixel);
};

ScaleManager.prototype.checkZoomOut = function(){
    var invisibleInterval = (this.end - this.start) - (this.visibleEnd-this.visibleStart);
    this.disableZoomOut = invisibleInterval <= this.zoomAccuracyMs;
    return !this.disableZoomOut;

};
ScaleManager.prototype.checkZoomIn = function(){
    this.disableZoomIn = this.zoom() <= this.fullZoomInValue();
};
ScaleManager.prototype.checkZoom = function(){
    this.checkZoomOut();
    this.checkZoomIn();
};
ScaleManager.prototype.checkZoomAsync = function(){
    var self = this;
    var result = self.$q.defer();
    setTimeout(function(){
        var oldDisableZoomOut = self.disableZoomOut;
        var oldDisableZoomIn = self.disableZoomIn;

        self.checkZoom();

        if(oldDisableZoomOut != self.disableZoomOut ||
            oldDisableZoomIn != self.disableZoomIn){
            result.resolve();
        }else{
            result.reject();
        }
    });
    return result.promise;
};
ScaleManager.prototype.zoom = function(zoomValue){ // Get or set zoom value (from 0 to 1)
    if(typeof(zoomValue)=="undefined"){
        return this.msToZoom(this.msPerPixel);
    }

    this.msPerPixel = this.zoomToMs(zoomValue);

    this.updateCurrentInterval();
};
ScaleManager.prototype.zoomAroundDate = function(zoomValue, aroundDate){
    var anchorDate = this.anchorDate;
    this.tryToRestoreAnchorDate(aroundDate); // try to set new anchorDate
    this.zoom(zoomValue);                    // zoom and update visible interval
    this.tryToRestoreAnchorDate(anchorDate); // try to restore anchorDate
};

ScaleManager.prototype.lastMinute = function(){
    return (new Date(this.end - this.lastMinuteInterval)).getTime();
};

ScaleManager.prototype.updateServerOffset = function(serverOffset){
    this.timeZoneOffset = serverOffset.timeZoneOffset;
    this.latency = serverOffset.latency;
};


/*
Input: date to be corrected
Output: date adjusted to server time
Summary: 1)We convert the input date into a Date object.
         2)We convert the timezone offset from minutes to ms.
         3)Add timezone offset to current time in ms.
         4)Adding the server's timezone and minus the latency sets the
           local time to the server's time.
*/
ScaleManager.prototype.serverTime = function(date){
    if(!this.useServerTime)
        return date;
    var currentTime = new Date(date);
    return (currentTime.getTime() + currentTime.getTimezoneOffset() * 60000) + this.timeZoneOffset - this.latency;
};
