(function () {
    
    'use strict';
    
    /*
        window.timeManager is for server/local time conversions.
        In data structures we always store 'display time'
    In server requests - we always use server time
    */
    window.timeManager = {
        useServerTime: true,
        useSystemTime: true,
        timeLatency: 0,
        timeZoneOffset: 0
    };
    window.timeManager.serverToLocal = function (date) {
        return Math.max(date + this.timeLatency, 0);
    };
    
    window.timeManager.localToServer = function (date) {
        return Math.max(date - this.timeLatency, 0);
    };
    
    window.timeManager.displayToServer = function (date) {
        if (this.useServerTime || this.useSystemTime) {
            return date + this.timeZoneOffset; // Translate server time to localtime
        }
        return this.localToServer(date);  // Save server time
    };
    window.timeManager.serverToDisplay = function (date) {
        if (this.useServerTime || this.useSystemTime) {
            return date - this.timeZoneOffset;  // Save server time
        }
        return this.serverToLocal(date); // Translate server time to localtime
    };
    
    
    window.timeManager.localToDisplay = function (date) {
        if (this.useServerTime || this.useSystemTime) {
            return this.serverToDisplay(this.localToServer(date));
        }
        return date;
    };
    window.timeManager.displayToLocal = function (date) {
        if (this.useServerTime || this.useSystemTime) {
            return this.serverToLocal(this.displayToServer(date));
        }
        return date;
    };
    window.timeManager.nowToDisplay = function () {
        return this.localToDisplay(this.nowLocal());
    };
    window.timeManager.nowToServer = function () {
        return this.localToServer(this.nowLocal());
    };
    
    window.timeManager.translate = function (date, reverse) {
        var latency = this.timeLatency;
        
        /*if(this.useServerTime){
            return date;
        }*/
        if (reverse) {
            return date + latency;
        }
        return date - latency;
    };
    window.timeManager.nowLocal = function () {
        return (new Date()).getTime();
    };
    window.timeManager.debug = function (message, date) {
        console.log(message, {
            display: new Date(date),
            toServer: new Date(this.displayToServer(date)),
            toLocal: new Date(this.displayToLocal(date))
        });
    };
    window.timeManager.getOffset = function (serverTime, serverTimeZoneOffset) {
        // Calculate server offset comparing with local time
        var minTimeLag = 2000;// Two seconds
        var clientDate = new Date();
        var clientTime = clientDate.getTime();
        var latency = 0;
        if (Math.abs(clientTime - serverTime) > minTimeLag) {
            latency = clientTime - serverTime;
        }
        return {
            latency: latency,
            serverTimeZoneOffset: serverTimeZoneOffset
        };
    };
    window.timeManager.setOffset = function (offset) {
        // Set previously calculated server offset
        this.timeLatency = offset.latency; // time latency handles the situation when time is not synchronised
        this.serverTimeZoneOffset = offset.serverTimeZoneOffset;
        this.timeZoneOffset = this.clientTimeZoneOffset - this.serverTimeZoneOffset;
    };
    window.timeManager.init = function (useServerTime, useSystemTime) {
        // Init time manager - get client's timezone
        this.useServerTime = useServerTime;
        this.useSystemTime = useSystemTime;
        var clientDate = new Date();
        this.clientTimeZoneOffset = -clientDate.getTimezoneOffset() * 60000;
        // For some reason local timezone offset has wrong sign, so we sum them instead of subtractions
    };

//Record
    window.Chunk = function (boundaries, start, end, level, title, extension) {
        this.start = start;
        this.end = end;
        this.level = level || 0;
        this.expand = true;
        
        var format = 'dd.mm.yyyy HH:MM:ss.l';
        this.title = (typeof (title) === 'undefined' || title === null) ? window.dateFormat(start, format) + ' - ' + window.dateFormat(end, format) : title;
        
        this.children = [];
        
        _.extend(this, extension);
    };
    window.Chunk.prototype.debug = function () {
        var format = 'dd.mm.yyyy HH:MM:ss.l';
        this.title = window.dateFormat(this.start, format) + ' - ' + window.dateFormat(this.end, format);
        
        console.log(new Array(this.level + 1).join(' '), this.title, this.level, this.children.length);
    };
    
    function isDate(val) {
        return val instanceof Date;
    }
    
    function NumberToDate(date) {
        if (typeof (date) === 'number' || typeof (date) === 'Number') {
            date = Math.round(date);
            date = new Date(date);
        }
        if (!isDate(date)) {
            console.error('non date ' + (typeof date) + ': ' + date);
            return null;
        }
        return date;
    }

// Additional mini-library for declaring and using settings for ruler levels
    window.Interval = function (ms, seconds, minutes, hours, days, months, years) {
        this.seconds = seconds;
        this.minutes = minutes;
        this.hours = hours;
        this.days = days;
        this.months = months;
        this.years = years;
        this.milliseconds = ms
    };
    window.Interval.prototype.addToDate = function (date, count) {
        date = new NumberToDate(date);
        
        if (typeof (count) === 'undefined') {
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
        } catch (error) {
            console.error('date problem', date);
            throw error;
        }
    };
    /**
     * How many seconds are there in the interval.
     * Fucntion may return not exact value, it doesn't count leap years (difference doesn't matter in that case)
     * @returns {*}
     */
    window.Interval.prototype.getMilliseconds = function () {
        var date1 = new Date(1971, 11, 1, 0, 0, 0, 0);//The first of december before the leap year. We need to count maximum interval (month -> 31 day, year -> 366 days)
        var date2 = this.addToDate(date1);
        return date2.getTime() - date1.getTime();
    };
    /**
     * Align to past. Can't work with intervals like '1 month and 3 days'
     * @param dateToAlign
     * @returns {Date}
     */
    window.Interval.prototype.alignToPast = function (dateToAlign) {
        var date = new Date(dateToAlign);
        
        if (this.milliseconds === 0) {
            date.setMilliseconds(0);
        } else {
            date.setMilliseconds(Math.floor(date.getMilliseconds() / this.milliseconds) * this.milliseconds);
            return date;
        }
        
        if (this.seconds === 0) {
            date.setSeconds(0);
        } else {
            date.setSeconds(Math.floor(date.getSeconds() / this.seconds) * this.seconds);
            return date;
        }
        
        if (this.minutes === 0) {
            date.setMinutes(0);
        } else {
            date.setMinutes(Math.floor(date.getMinutes() / this.minutes) * this.minutes);
            return date;
        }
        
        if (this.hours === 0) {
            date.setHours(0);
        } else {
            date.setHours(Math.floor(date.getHours() / this.hours) * this.hours);
            return date;
        }
        
        if (this.days === 0) {
            date.setDate(1);
        } else {
            date.setDate(Math.floor(date.getDate() / this.days) * this.days);
            return date;
        }
        
        if (this.months === 0) {
            date.setMonth(0);
        } else {
            date.setMonth(Math.floor(date.getMonth() / this.months) * this.months);
            return date;
        }
        
        date.setYear(Math.floor(date.getFullYear() / this.years) * this.years);
        return date;
    };
//Check if current date aligned by interval
    window.Interval.prototype.checkDate = function (date) {
        date = new NumberToDate(date);
        
        if (!date) {
            return false;
        }
        
        return this.alignToPast(date).getTime() === date.getTime();
    };
    window.Interval.prototype.alignToFuture = function (date) {
        return this.alignToPast(this.addToDate(date));
    };
    
    
    /**
     * Special model for ruler levels. Stores all possible detailization level for the ruler
     * @type {{levels: *[], getLevelIndex: getLevelIndex}}
     */
    window.RulerModel = {
        
        /**
         * Presets for detailization levels
         * @type {{detailization: number}[]}
         */
        levels: [
            {
                name: 'Age',
                interval: new window.Interval(0, 0, 0, 0, 0, 0, 100),
                format: 'yyyy',
                marksW: 15,
                smallW: 40,
                middleW: 400,
                width: 4000,
                topW: 0,
                topFormat: 'yyyy'
            }, // root
            {
                name: 'Decade',
                interval: new window.Interval(0, 0, 0, 0, 0, 0, 10),
                format: 'yyyy',
                marksW: 15,
                smallW: 40,
                middleW: 400,
                width: 9000
            },
            {
                name: 'Year', //Years
                format: 'yyyy',//Format string for date
                interval: new window.Interval(0, 0, 0, 0, 0, 0, 1),// Interval for marks
                marksW: 15,   // width for marks without label
                smallW: 40,  // width for smallest label
                middleW: 120, // width for middle label
                width: 720, // width for label. We should choose minimal width in order to not intresect labels on timeline
                topW: 100, // minimal width for label above timeline
                topFormat: 'yyyy'//Format string for label above timeline
            },
            {
                name: '6Months',
                interval: new window.Interval(0, 0, 0, 0, 0, 6, 0),
                format: 'mmmm',
                marksW: 15,
                smallW: 60,
                middleW: 360,
                width: 3600
            },
            {
                name: '3Months',
                interval: new window.Interval(0, 0, 0, 0, 0, 3, 0),
                format: 'mmmm',
                marksW: 15,
                smallW: 60,
                middleW: 1800,
                width: 9000
            },
            {
                name: 'Month',
                interval: new window.Interval(0, 0, 0, 0, 0, 1, 0),
                format: 'mmmm',
                marksW: 15,
                smallW: 60,
                middleW: 100000,
                width: 600,
                topW: 170,
                topFormat: 'mmmm yyyy'
            },
            {
                name: 'Day',
                interval: new window.Interval(0, 0, 0, 0, 1, 0, 0),
                format: 'dd',
                marksW: 5,
                smallW: 20,
                middleW: 100,
                width: 200,
                topW: 170,
                topFormat: 'd mmmm yyyy',
                contained: 1
            },
            {
                name: '12h',
                interval: new window.Interval(0, 0, 0, 12, 0, 0, 0),
                format: window.TimelineConfig.hourFormat,
                marksW: 15,
                smallW: 50,
                middleW: 200,
                width: 1200
            },
            {
                name: '6h',
                interval: new window.Interval(0, 0, 0, 6, 0, 0, 0),
                format: window.TimelineConfig.hourFormat,
                marksW: 15,
                smallW: 50,
                middleW: 600,
                width: 1800
            },
            {
                name: '3h',
                interval: new window.Interval(0, 0, 0, 3, 0, 0, 0),
                format: window.TimelineConfig.hourFormat,
                marksW: 15,
                smallW: 50,
                middleW: 300,
                width: 900
            },
            {
                name: '1h',
                interval: new window.Interval(0, 0, 0, 1, 0, 0, 0),
                format: window.TimelineConfig.hourFormat,
                marksW: 15,
                smallW: 50,
                middleW: 100,
                width: 300
            },
            {
                name: '30m',
                interval: new window.Interval(0, 0, 30, 0, 0, 0, 0),
                format: window.TimelineConfig.hourFormat,
                marksW: 15,
                smallW: 50,
                middleW: 300,
                width: 1500
            },
            {
                name: '10m',
                interval: new window.Interval(0, 0, 10, 0, 0, 0, 0),
                format: window.TimelineConfig.hourFormat,
                marksW: 15,
                smallW: 50,
                middleW: 500,
                width: 1000
            },
            {
                name: '5m',
                interval: new window.Interval(0, 0, 5, 0, 0, 0, 0),
                format: window.TimelineConfig.hourFormat,
                marksW: 15,
                smallW: 50,
                middleW: 250,
                width: 500
            },
            {
                name: '1m',
                interval: new window.Interval(0, 0, 1, 0, 0, 0, 0),
                format: window.TimelineConfig.hourFormat,
                marksW: 15,
                smallW: 50,
                middleW: 100,
                width: 300,
                topW: 170,
                topFormat: window.TimelineConfig.dateFormat + ' ' + window.TimelineConfig.hourFormat
            },
            {
                name: '30s',
                interval: new window.Interval(0, 30, 0, 0, 0, 0, 0),
                format: 'ss"s"',
                marksW: 15,
                smallW: 50,
                middleW: 300,
                width: 1500
            },
            {
                name: '10s',
                interval: new window.Interval(0, 10, 0, 0, 0, 0, 0),
                format: 'ss"s"',
                marksW: 15,
                smallW: 50,
                middleW: 500,
                width: 1000
            },
            {
                name: '5s',
                interval: new window.Interval(0, 5, 0, 0, 0, 0, 0),
                format: 'ss"s"',
                marksW: 15,
                smallW: 50,
                middleW: 250,
                width: 100000
            },
            {
                name: '1s',
                interval: new window.Interval(0, 1, 0, 0, 0, 0, 0),
                format: 'ss"s"',
                marksW: 15,
                smallW: 50,
                middleW: 100000,
                width: 100000,
                topW: 200,
                topFormat: window.TimelineConfig.dateFormat + ' ' + window.TimelineConfig.timeFormat
            }
            //{ name:'500ms'     , interval:  new Interval(500, 0, 0, 0, 0, 0, 0), format:'l"ms"', marksW:15, smallW: 50, middleW: 250,      width: 100000},
            //{ name:'100ms'     , interval:  new Interval(100, 0, 0, 0, 0, 0, 0), format:'l"ms"', marksW:15, smallW: 50, middleW: 100000,   width: 100000 }
        ],
        
        getLevelIndex: function (searchdetailization, width) {
            width = width || 1;
            var targetLevel = _.find(window.RulerModel.levels, function (level) {
                return level.interval.getMilliseconds() < searchdetailization / width;
            });
            
            return typeof (targetLevel) !== 'undefined' ? this.levels.indexOf(targetLevel) : this.levels.length - 1;
        },
        
        
        findBestLevelIndex: function (date, maxLevelIndex) {
            if (maxLevelIndex) {
                var maxLevel = Math.min(maxLevelIndex - 1, this.levels.length - 1);
                
                for (var i = maxLevel; i >= 0; i--) {
                    if (!this.levels[i].interval.checkDate(date)) {
                        return i + 1;
                    }
                }
                return i;
            }
            
            for (var idx = 0; idx < this.levels.length; idx++) {
                if (this.levels[idx].interval.checkDate(date)) {
                    return idx;
                }
            }
            return this.levels.length - 1;
            
        },
        findBestLevel: function (date) {
            return this.levels[this.findBestLevelIndex(date)];
        }
    };

//Provider for records from mediaserver
    window.CameraRecordsProvider = function (cameras, mediaserver, width) {
        this.cameras = cameras;
        this.width = width;
        this.mediaserver = mediaserver;
    };
    
    window.CameraRecordsProvider.prototype.init = function () {
        this.chunksTree = null;
        this.requestedCache = [];
        var self = this;
        //1. request first detailization to get initial bounds
        
        this.lastRequested = window.timeManager.nowToServer(); // lastrequested is always servertime
        return this.requestInterval(0, this.lastRequested + 10000, 0).then(function () {
            if (!self.chunksTree) {
                return false; //No chunks for this camera
            }
            
            // Depends on this interval - choose minimum interval, which contains all records and request deeper detailization
            var nextLevel = window.RulerModel.getLevelIndex(window.timeManager.nowToDisplay() - self.chunksTree.start, self.width);
            
            if (nextLevel < window.RulerModel.levels.length - 1) {
                nextLevel++;
            }
            return self.requestInterval(window.timeManager.displayToServer(self.chunksTree.start),
                window.timeManager.nowToServer(),
                nextLevel).then(function () {
                return true;
            });
        });
    };
    window.CameraRecordsProvider.prototype.cacheRequestedInterval = function (start, end, level) {
        for (var i = 0; i < level + 1; i++) {
            if (i >= this.requestedCache.length) {
                this.requestedCache.push([{start: start, end: end}]); //Add new cache level
                continue;
            }
            
            //Find good position to cache new requested interval
            for (var j = 0; j < this.requestedCache[i].length; j++) {
                if (this.requestedCache[i][j].end < start) {
                    continue;
                }
                
                if (this.requestedCache[i][j].start > end) { // no intersection - just add to the beginning
                    this.requestedCache[i].splice(j, 0, {start: start, end: end});
                    break;
                }
                
                // Intersection - unite two intervals together
                this.requestedCache[i][j].start = Math.min(start, this.requestedCache[i][j].start);
                this.requestedCache[i][j].end = Math.max(end, this.requestedCache[i][j].end);
                break;
            }
            if (j === this.requestedCache[i].length) {
                // We need to add our requested interval to the end of an array
                this.requestedCache[i].push({start: start, end: end});
            }
        }
    };
    window.CameraRecordsProvider.prototype.getBlindSpotsInCache = function (start, end, level) {
        if (typeof (this.requestedCache[level]) === 'undefined') {
            return [{start: start, end: end, blindSpotsInCache: true}]; // One large blind spot
        }
        var result = [];
        
        var levelCache = this.requestedCache[level];
        for (var i = 0; i < levelCache.length; i++) {
            if (levelCache[i].end < start) {
                continue; // Ignore this cached interval - not relevant
            }
            
            if (levelCache[i].start > start) { // Calculate blank spot
                result.push({start: start, end: Math.min(levelCache[i].start, end), blindSpotsInCache: true});
            }
            
            start = levelCache[i].end; // start of the next blind spot
            
            if (start >= end) { // Time to stop, we got the whole interval covered
                break;
            }
        }
        if (start < end) { // We did not cover the whole interval - so we add one more blind spot
            result.push({start: start, end: end, blindSpotsInCache: true});
        }
        return result;
    };

//Used by ShortCache and CameraRecords
    function parseChunks(chunks) {
        return _.forEach(chunks, function (chunk) {
            chunk.durationMs = parseInt(chunk.durationMs);
            chunk.startTimeMs = window.timeManager.serverToDisplay(parseInt(chunk.startTimeMs));
            
            if (chunk.durationMs === -1) {
                chunk.durationMs = window.timeManager.nowToDisplay() - chunk.startTimeMs;//in future
            }
        });
    }
    
    window.CameraRecordsProvider.prototype.checkRequestedIntervalCache = function (start, end, level) {
        if (typeof (this.requestedCache[level]) === 'undefined') {
            return false;
        }
        var levelCache = this.requestedCache[level];
        
        for (var i = 0; i < levelCache.length; i++) {
            if (start < levelCache[i].start) { // there is a blind spot in cache
                return false;
            }
            
            start = Math.max(start, levelCache[i].end); // Move start
            
            if (end <= start) { // No need to move forward - there is no blind spot in cache
                return true;
            }
        }
        return end <= start;
    };
    window.CameraRecordsProvider.prototype.updateLastMinute = function (lastMinuteDuration, level) {
        var now = window.timeManager.nowToServer();
        
        if (now - this.lastRequested > lastMinuteDuration / 2) {
            this.requestInterval(now - lastMinuteDuration, now + 10000, level);
            this.lastRequested = now;
        }
    };
    window.CameraRecordsProvider.prototype.abort = function (reason) {
        if (this.currentRequest) {
            this.currentRequest.abort(reason);
            this.currentRequest = null;
        }
    };
    window.CameraRecordsProvider.prototype.requestInterval = function (start, end, level) {
        if (start > end) {
            console.error('Start is more than end, that is impossible');
        }
        this.level = level;
        if (this.currentRequest) {
            return this.currentRequest;
            // that logic might be wrong, since currentRequest might be different from new request
        }
        
        var levelData = window.RulerModel.levels[level];
        var detailization = levelData.interval.getMilliseconds();
        
        var self = this;
        
        //1. Request records for interval
        
        /*
         Here is the logic behind this weird staff:
         self.currentRequest remembers last request we sent, it supposed to prevent the code from making two requests
         at the same time.
         But due to race condition the problem might appear and another request will be sent. In this case we will
         receive two responses instead of one. That's why we use makeRequest to remember the initial request and compare
         with self.currentRequest in the handler.
         */
        var makeRequest = this.mediaserver.getRecords(this.cameras[0], start, end, detailization, null, levelData.name);
        self.currentRequest = makeRequest;
        return self.currentRequest.then(function (data) {
            if (makeRequest !== self.currentRequest) {
                console.error('Unexpected response in CameraRecordsProvider', data);
                return;
            }
            self.currentRequest = null;//Unlock requests - we definitely have chunkstree here
            var chunks = parseChunks(data.data.reply);
            
            var chunksToIterate = chunks.length;
            //If level == 0 - we want only first chunk
            if (!level && chunks.length > 0) { // Special hack
                chunks[0].durationMs = -1; // Make it 'live'
                chunksToIterate = 1;
            }
            for (var i = 0; i < chunksToIterate; i++) {
                var endChunk = chunks[i].startTimeMs + chunks[i].durationMs;
                if (chunks[i].durationMs < 0 || endChunk > window.timeManager.nowToDisplay()) {
                    endChunk = window.timeManager.nowToDisplay();// current moment
                }
                
                if (chunks[i].startTimeMs > endChunk) {
                    continue; // Chunk is from future - do not add
                }
                
                var addchunk = new window.Chunk(null, chunks[i].startTimeMs, endChunk, level);
                self.addChunk(addchunk, null);
            }
            
            self.cacheRequestedInterval(window.timeManager.serverToDisplay(start), window.timeManager.serverToDisplay(end), level);
            
            return self.chunksTree;
        });
    };
    /**
     * Request records for interval, add it to cache, update visibility splice
     *
     * @param start
     * @param end
     * @param level
     * @return Array
     */
    window.CameraRecordsProvider.prototype.getIntervalRecords = function (start, end, level, debugLevel) {
        
        if (start instanceof Date) {
            start = start.getTime();
        }
        if (end instanceof Date) {
            end = end.getTime();
        }
        
        // Splice existing intervals and check, if we need an update from server
        var result = [];
        this.selectRecords(result, start, end, level, null, debugLevel);
        
        /*this.logcounter = this.logcounter||0;
        this.logcounter ++;
        if(this.logcounter % 1000 === 0) {
                log('splice: ============================================================');
            for (var i = 0; i < result.length; i++) {
                result[i].debug();
            }
            this.debug();
        }*/
        
        
        if (window.timeManager.displayToServer(end) > this.lastRequested) { // If we want data from future
            end = window.timeManager.serverToDisplay(this.lastRequested); // Limit the request with latest lastminute update
            if (start > end) {  // if start is in future as well - just return what we have
                return result;
            }
        }
        
        var needUpdate = !debugLevel && !this.checkRequestedIntervalCache(start, end, level);
        if (needUpdate) { // Request update
            this.requestInterval(window.timeManager.displayToServer(start), window.timeManager.displayToServer(end), level);
        }
        // Return splice - as is
        return result;
    };
    window.CameraRecordsProvider.prototype.debug = function (currentNode, depth) {
        if (!currentNode) {
            console.log('Chunks tree:' + (this.chunksTree ? '' : 'empty'));
        }
        if (typeof (depth) === 'undefined') {
            depth = window.RulerModel.levels.length - 1;
        }
        currentNode = currentNode || this.chunksTree;
        
        if (currentNode) {
            currentNode.debug();
            if (depth > 0) {
                for (var i = 0; i < currentNode.children.length; i++) {
                    this.debug(currentNode.children[i], depth - 1);
                }
            }
        }
    };
    window.CameraRecordsProvider.prototype.debugSplice = function (events, level) {
        console.log('Events:', level, events.length);
        for (var i = 0; i < events.length; i++) {
            events[i].debug();
        }
        console.log('/ Events:', level, events.length);
    };
    /**
     * Add chunk to tree - find or create good position for it
     * @param chunk
     * @param parent - parent for recursive callрп
     */
    window.CameraRecordsProvider.prototype.addChunk = function (chunk, parent) {
        if (this.chunksTree === null || chunk.level === 0) {
            this.chunksTree = chunk;
            return;
        }
        
        parent = parent || this.chunksTree;
        
        if (chunk.level <= parent.level) {
            console.error('something wrong happened', chunk, parent);
            return;
        }
        
        if (parent.end < chunk.end) {
            parent.end = chunk.end;
        }
        
        // Go through tree, find good place for chunk
        if (parent.children.length === 0) { // no children yet
            if (parent.level === chunk.level - 1) { //
                parent.children.push(chunk);
                return;
            }
            parent.children.push(new window.Chunk(parent, chunk.start, chunk.end, parent.level + 1));
            this.addChunk(chunk, parent.children[0]);
            return;
        }
        
        for (var i = 0; i < parent.children.length; i++) {
            var iteratingChunk = parent.children[i];
            
            if (iteratingChunk.end < chunk.start) { //no intersection - no way we need this chunk now
                continue;
            }
            
            if (iteratingChunk.end >= chunk.end && iteratingChunk.start <= chunk.start) { // Contained chunk
                if (iteratingChunk.level !== chunk.level) { // Add inside ot ignore
                    this.addChunk(chunk, iteratingChunk);
                }
                return;
            }
            
            if (iteratingChunk.start < chunk.end) { // Intersection here!
                // Cut chunk into pieces!
                if (chunk.start < iteratingChunk.start) {
                    iteratingChunk.start = chunk.start;
                }
                
                if (chunk.end > iteratingChunk.end) {
                    if (!parent.children[i + 1] || parent.children[i + 1].start >= chunk.end) {
                        iteratingChunk.end = chunk.end;
                    } else {
                        // TODO: WTF is going here?
                        var rightChunk = new window.Chunk(null, iteratingChunk.end + 1, chunk.end, chunk.level);
                        chunk.end = iteratingChunk.end;
                        this.addChunk(rightChunk, parent);
                    }
                }
                
                if (iteratingChunk.level !== chunk.level) { // Add inside ot ignore
                    this.addChunk(chunk, iteratingChunk);
                }
                return;
            }
            
            // No intersection - add here
            if (parent.level === chunk.level - 1) {
                parent.children.splice(i, 0, chunk);//Just add chunk here and return
            } else {
                parent.children.splice(i, 0, new window.Chunk(parent, chunk.start, chunk.end, parent.level + 1)); //Create new children and go deeper
                this.addChunk(chunk, parent.children[i]);
            }
            return;
        }
        
        if (parent.level === chunk.level - 1) {
            parent.children.push(chunk);
        } else {
            //Add new last chunk and go deeper
            parent.children.push(new window.Chunk(parent, chunk.start, chunk.end, parent.level + 1)); //Create new chunk
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
    window.CameraRecordsProvider.prototype.selectRecords = function (result, start, end, level, parent, debugLevel) {
        parent = parent || this.chunksTree;
        
        if (!parent) {
            return;
        }
        
        if (parent.end <= start || parent.start >= end) { // Not intresected
            return;
        }
        if (parent.level === level) {
            result.push(parent); // Good chunk!
            return;
        }
        
        if (parent.children.length === 0) {
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
        var blindSpots = this.getBlindSpotsInCache(start, end, parent.level + 1);
        
        for (i = 0; i < blindSpots.length; i++) {
            result.push(blindSpots[i]);
        }
    };
    
    /**
     * ShortCache - special collection for short chunks with best detailization for calculating playing position and date
     * @constructor
     */
    window.ShortCache = function (cameras, mediaserver) {
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
    };
    window.ShortCache.prototype.init = function (start, isPlaying) {
        this.isReady = false; // use a flag to indicate initialization end
        this.liveMode = false;
        if (!start) {
            this.liveMode = true;
            start = window.timeManager.nowToDisplay();
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
        this.playing = typeof (isPlaying) !== 'undefined' ? isPlaying : true;
        
        this.update();
    };
    window.ShortCache.prototype.abort = function (reason) {
        if (this.currentRequest) {
            this.currentRequest.abort(reason);
            this.currentRequest = null;
        }
    };
    window.ShortCache.prototype.update = function (requestPosition, position) {
        this.isReady = false;
        //Request from current moment to 1.5 minutes to future
        
        // Save them to this.currentDetailization
        if (this.updating && !requestPosition) { //Do not send request twice
            return;
        }
        
        requestPosition = Math.round(requestPosition) || Math.round(this.playedPosition);
        
        this.updating = true;
        var self = this;
        this.lastRequestDate = requestPosition;
        this.lastRequestPosition = position || this.played;
        
        this.abort('update again');
        // Get next {{limitChunks}} chunks
        
        /*
        Here is the logic behind this weird staff:
        self.currentRequest remembers last request we sent, it supposed to prevent the code from making two requests
        at the same time.
        But due to race condition the problem might appear and another request will be sent. In this case we will
        receive two responses instead of one. That's why we use makeRequest to remember the initial request and compare
        with self.currentRequest in the handler.
        */
        
        var makeRequest = this.mediaserver.getRecords(
            this.cameras[0],
            window.timeManager.displayToServer(requestPosition),
            window.timeManager.nowToServer() + 100000,
            this.requestDetailization,
            this.limitChunks
        );
        this.currentRequest = makeRequest;
        
        this.currentRequest.then(function (data) {
            
            if (makeRequest !== self.currentRequest) {
                self.isReady = undefined;
                console.error('Unexpected response in ShortCache', data);
                return;
            }
            self.updating = false;
            self.currentRequest = null;
            
            var chunks = parseChunks(data.data.reply);
            
            //if(chunks.length == 0){ } // no chunks for this camera and interval
            
            
            if (chunks.length > 0 && chunks[0].startTimeMs < requestPosition) { // Crop first chunk.
                chunks[0].durationMs += chunks[0].startTimeMs - requestPosition;
                chunks[0].startTimeMs = requestPosition;
            }
            self.currentDetailization = chunks;
            
            var lastCheckPointDate = self.lastRequestDate;
            var lastCheckPointPosition = self.lastRequestPosition;
            var lastSavedCheckpoint = 0; // First chunk will be forced to save
            
            for (var i = 0; i < self.currentDetailization.length; i++) {
                lastCheckPointPosition += self.currentDetailization[i].durationMs;// Duration of chunks count
                lastCheckPointDate = self.currentDetailization[i].startTimeMs + self.currentDetailization[i].durationMs;
                
                if (lastCheckPointDate - lastSavedCheckpoint > self.checkpointsFrequency) {
                    lastSavedCheckpoint = lastCheckPointDate;
                    self.checkPoints[lastCheckPointPosition] = lastCheckPointDate; // Too many checkpoints at this point
                }
            }
            
            self.setPlayingPosition(self.played);
            self.isReady = true;
        }, function (error) {
            if (!makeRequest.abortReason) { // If the request was aborted - no handling is needed
                self.isReady = false;
                console.error('Was not able to get records from server', error);
            }
        });
    };

// Check playing date - return videoposition if possible
    window.ShortCache.prototype.checkPlayingDate = function (positionDate) {
        if (positionDate < this.start) { //Check left boundaries
            return false; // Return negative value - outer code should request new videocache
        }
        
        if (positionDate > this.lastPlayedDate) {
            return false;
        }
        
        var lastPosition;
        for (var key in  this.checkPoints) {
            if (this.checkPoints.hasOwnProperty(key)) {
                if (this.checkPoints[key] > positionDate) {
                    break;
                }
                lastPosition = key;
            }
        }
        
        if (typeof (lastPosition) === 'undefined') {// No checkpoints - go to live
            return positionDate - this.start;
        }
        
        return lastPosition + positionDate - this.checkPoints[key]; // Video should jump to this position
    };
    window.ShortCache.prototype.setPlayingPosition = function (position) {
        // This function translate playing position (in millisecond) into actual date. Should be used while playing video only. Position must be in current buffered video
        
        if (this.liveMode) { // In live mode ignore whatever they have to say
            this.playedPosition = window.timeManager.nowToDisplay();
            return;
        }
        var oldPosition = this.playedPosition;
        
        this.played = position;
        this.playedPosition = 0;
        
        if (position < this.lastRequestPosition) { // Somewhere back on timeline
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
        for (var i = 0; i < this.currentDetailization.length; i++) {
            intervalToEat -= this.currentDetailization[i].durationMs;// Duration of chunks count
            this.playedPosition = this.currentDetailization[i].startTimeMs + this.currentDetailization[i].durationMs + intervalToEat;
            if (intervalToEat <= 0) {
                break;
            }
        }
        
        //this.liveMode = false;
        if (i === this.currentDetailization.length) { // We have no good detailization for this moment - pretend to be playing live
            
            if (!this.updating) {
                this.playedPosition = window.timeManager.nowToDisplay();
                this.liveMode = true;
            }
        }
        
        if (!this.liveMode && this.currentDetailization.length > 0) {
            var archiveEnd = this.currentDetailization[this.currentDetailization.length - 1].durationMs +
                this.currentDetailization[this.currentDetailization.length - 1].startTimeMs;
            if (archiveEnd < Math.round(this.playedPosition) + this.updateInterval && this.lastArchiveEnd !== archiveEnd) {
                // It's time to update
                // And last update get new information
                this.lastArchiveEnd = archiveEnd;
                this.update();
            }
        }
        
        if (position > this.lastPlayedPosition) {
            this.lastPlayedPosition = position; // Save the boundaries of uploaded cache
            this.lastPlayedDate = this.playedPosition; // Save the boundaries of uploaded cache
        }
        
        if (oldPosition > this.playedPosition && window.Config.allowDebugMode) {
            console.error('Position jumped back! ms:', oldPosition - this.playedPosition);
        }
        return this.playedPosition;
    };
    window.ShortCache.prototype.checkEndOfArchive = function () {
        var self = this;
        return this.mediaserver.getRecords(
            this.cameras[0],
            window.timeManager.displayToServer(this.playedPosition),
            window.timeManager.nowToServer() + 100000,
            this.requestDetailization,
            window.Config.webclient.chunksToCheckFatal
        ).then(function (data) {
            var chunks = parseChunks(data.data.reply);
            //If there are no chunks in the short cache use lastMinute
            var endDate = window.timeManager.nowToDisplay - window.TimelineConfig.lastMinuteDuration;
            if (chunks.length > 0) {
                //This is supposed to find the cutoff point in the chunk
                var endTime = window.Config.webclient.endOfArchiveTime;
                var i = chunks.length - 1;
                for (; i > 0; --i) {
                    if (endTime - chunks[i].durationMs <= 0) {
                        break;
                    }
                    endTime -= chunks[i].durationMs;
                }
                endDate = chunks[i].startTimeMs + chunks[i].durationMs - endTime;
            }
            return self.playedPosition > endDate;
        }, function () {
            return null;
        });
    };
    window.ShortCache.prototype.isArchiveEmpty = function () {
        return this.currentDetailization.length < 1;
    };
    
    window.ScaleManager = function (minMsPerPixel, maxMsPerPixel, defaultIntervalInMS, initialWidth, stickToLiveMs, zoomAccuracyMs,
                                    lastMinuteInterval, minPixelsPerLevel, minScrollBarWidth, pixelAspectRatio, $q) {
        this.absMaxMsPerPixel = maxMsPerPixel;
        this.minMsPerPixel = minMsPerPixel;
        this.minScrollBarWidth = minScrollBarWidth;
        this.stickToLiveMs = stickToLiveMs;
        this.zoomAccuracyMs = zoomAccuracyMs;
        this.minPixelsPerLevel = minPixelsPerLevel;
        this.lastMinuteInterval = lastMinuteInterval;
        this.pixelAspectRatio = pixelAspectRatio || 1;
        this.$q = $q;
        
        this.watch = {
            playing: false,
            live: false,
            forcedToStop: false
        };
        this.dragDate = null;
        
        
        this.levels = {
            top: {index: 0, level: window.RulerModel.levels[0]},
            labels: {index: 0, level: window.RulerModel.levels[0]},
            middle: {index: 0, level: window.RulerModel.levels[0]},
            small: {index: 0, level: window.RulerModel.levels[0]},
            marks: {index: 0, level: window.RulerModel.levels[0]},
            events: {index: 0, level: window.RulerModel.levels[0]}
        };

// Setup total boundaries
        this.viewportWidth = initialWidth;
        this.end = window.timeManager.nowToDisplay();
        this.start = this.end - defaultIntervalInMS;
        this.updateTotalInterval();

// Setup default scroll window
        this.msPerPixel = this.maxMsPerPixel;
        this.anchorPoint = 1;
        this.anchorDate = this.end;
        this.updateCurrentInterval();
    };
    window.ScaleManager.prototype.updateTotalInterval = function () {
        //Calculate maxmxPerPixel
        this.maxMsPerPixel = (this.end - this.start) / this.viewportWidth;
        this.msPerPixel = Math.min(this.msPerPixel, this.maxMsPerPixel);
    };
    window.ScaleManager.prototype.setViewportWidth = function (width) { // For initialization and window resize
        this.viewportWidth = width;
        this.updateTotalInterval();
        this.updateCurrentInterval();
    };
    window.ScaleManager.prototype.setStart = function (start) {// Update the begining end of the timeline. Live mode must be supported here
        this.start = start; //Start is always right
        this.updateTotalInterval();
    };
    window.ScaleManager.prototype.setEnd = function () { // Update right end of the timeline. Live mode must be supported here
        var needZoomOut = this.checkZoomedOutNow();
        
        var end = window.timeManager.nowToDisplay();
        if (this.playedPosition >= this.end || this.playedPosition >= end) {
            // Something strange is happening here - playing position in future
            end = this.playedPosition;
        }
        
        this.end = end;
        
        this.updateTotalInterval();
        if (needZoomOut) {
            this.zoom(1);
        }
    };
    window.ScaleManager.prototype.bound = function (min, val, max) {
        if (min > max) {
            //    console.warn('screwed bound');
            // var i = max;
            // max = min;
            // min = i;
        }
        val = Math.max(val, min);
        return Math.min(val, max);
    };
    window.ScaleManager.prototype.updateCurrentInterval = function () {
        //Calculate visibleEnd and visibleStart
        this.msPerPixel = this.bound(this.minMsPerPixel, this.msPerPixel, this.maxMsPerPixel);
        this.anchorPoint = this.bound(0, this.anchorPoint, 1);
        this.anchorDate = this.bound(this.start, Math.round(this.anchorDate), this.end);
        
        this.visibleStart = Math.round(this.anchorDate - this.msPerPixel * this.viewportWidth * this.anchorPoint);
        this.visibleEnd = Math.round(this.anchorDate + this.msPerPixel * this.viewportWidth * (1 - this.anchorPoint));
        
        
        if (this.visibleStart < this.start) {
            //Move end
            this.visibleEnd += this.start - this.visibleStart;
            this.visibleStart = this.start;
        }
        
        if (this.visibleEnd > this.end) {
            //Move end
            this.visibleStart += this.end - this.visibleEnd;
            this.visibleEnd = this.end;
        }
        
        if (this.visibleStart > this.visibleEnd) {
            console.error('wtf! visibleStart > visibleEnd - do full zoom out', new Date(this.visibleStart), new Date(this.visibleEnd));
            this.visibleStart = this.start;
            this.visibleEnd = this.end;
            this.msPerPixel = this.maxMsPerPixel;
        }
        
        // Adjust here with msPerPixel again!
        this.updateLevels();
    };
    window.ScaleManager.prototype.stopWatching = function () {
        this.watch.playing = false;
        this.watch.live = false;
        this.watch.forcedToStop = true;
    };
    window.ScaleManager.prototype.releaseWatching = function () {
        this.watch.forcedToStop = false;
    };
    window.ScaleManager.prototype.watchPosition = function (date, livePosition) {
        this.watch.playing = !livePosition;
        this.watch.live = livePosition;
        this.watch.forcedToStop = false;
        
        if (!date) { // Date not specified - do nothing
            return;
        }
        var targetPoint = this.dateToScreenCoordinate(date) / this.viewportWidth;
        if (targetPoint > 1) {  // Try to set playing position in the middle of the screen
            targetPoint = 0.5;
        }
        this.setAnchorDateAndPoint(date, targetPoint);
    };
    window.ScaleManager.prototype.checkWatch = function (liveOnly) {
        if (!liveOnly && this.watch.forcedToStop) {
            return;  // was forced to stop - wait for release
        }
        
        // Logic:
        // We are playing?
        //      yes: playing position is visible?
        //              yes: watch playing and return
        //      no: live position is visible or close to right border?
        //           yes: watch live and return
        
        if (!liveOnly && this.playing) {
            // check current playing position is visible
            var playingPoint = this.dateToScreenCoordinate(this.playedPosition);
            if (0 <= playingPoint && playingPoint <= this.viewportWidth - 1) { // playing position is visible on the screen
                return this.watchPosition(this.playedPosition, false);
            }
        }
        
        // playing position is outside the screen - try to watch live
        if (this.end - this.visibleEnd < this.stickToLiveMs) {
            // Watch live!
            return this.watchPosition(this.end, true);
        }
    };
    window.ScaleManager.prototype.updateWatching = function () {
        if (this.watch.forcedToStop) {
            return; // Was forced to stop - do nothing
        }
        if (this.watch.live) { // watching live position - set anchor to end
            return this.setAnchorDateAndPoint(this.end, 1);
        }
        if (this.watch.playing) { // watching position - update anchor point
            return this.setAnchorDateAndPoint(this.playedPosition, this.anchorPoint); //updateWatching
        }
        this.checkWatch(); // not watching now - check if we can now
    };
    window.ScaleManager.prototype.updatePlayingState = function (playedPosition, liveMode, playing) {
        this.liveMode = liveMode;
        this.playing = playing;
        if (playedPosition !== null) {
            this.playedPosition = playedPosition;
        }
        
        this.setEnd();
        this.updateWatching();
    };
    window.ScaleManager.prototype.tryToRestoreAnchorDate = function (date) {
        var targetPoint = this.dateToScreenCoordinate(date) / this.viewportWidth;
        if (0 <= targetPoint && targetPoint <= 1) {
            //this.setAnchorDateAndPoint(date, targetPoint);
            this.anchorDate = date;
            this.anchorPoint = targetPoint;
        }
    };
    window.ScaleManager.prototype.clickedCoordinate = function (coordinate) {
        if (typeof (coordinate) !== 'undefined') {
            this.clickedCoordinateValue = coordinate;
        }
        return this.clickedCoordinateValue;
    };
    window.ScaleManager.prototype.setAnchorCoordinate = function (coordinate) { // Set anchor date
        var position = this.screenCoordinateToDate(coordinate);
        this.setAnchorDateAndPoint(position, coordinate / this.viewportWidth);
        return position;
    };
    window.ScaleManager.prototype.setAnchorDateAndPoint = function (date, point) { // Set anchor date
        this.anchorDate = date;
        if (typeof (point) !== 'undefined') {
            this.anchorPoint = point;
        }
        this.updateCurrentInterval();
    };
    window.ScaleManager.prototype.fullZoomOutValue = function () {
        return this.msToZoom(this.maxMsPerPixel);
    };
    window.ScaleManager.prototype.fullZoomInValue = function () {
        return this.msToZoom(this.minMsPerPixel);
    };
    window.ScaleManager.prototype.boundZoom = function (zoomTarget) {
        return this.bound(this.fullZoomInValue(), zoomTarget, this.fullZoomOutValue());
    };
    window.ScaleManager.prototype.calcLevels = function (msPerPixel) {
        var zerolevel = window.RulerModel.levels[0];
        var levels = {
            top: {index: 0, level: zerolevel},
            labels: {index: 0, level: zerolevel},
            middle: {index: 0, level: zerolevel},
            small: {index: 0, level: zerolevel},
            marks: {index: 0, level: zerolevel},
            events: {index: 0, level: zerolevel}
        };
        for (var i = 0; i < window.RulerModel.levels.length; i++) {
            var level = window.RulerModel.levels[i];
            
            var pixelsPerLevel = (level.interval.getMilliseconds() / msPerPixel);
            
            if (typeof (level.topW) !== 'undefined' &&
                pixelsPerLevel >= level.topW) {
                levels.top = {index: i, level: level};
            }
            
            if (pixelsPerLevel >= level.width) {
                levels.labels = {index: i, level: level};
            }
            
            if (pixelsPerLevel >= level.middleW) {
                levels.middle = {index: i, level: level};
            }
            
            if (pixelsPerLevel >= level.smallW) {
                levels.small = {index: i, level: level};
            }
            
            if (pixelsPerLevel >= level.marksW) {
                levels.marks = {index: i, level: level};
            }
            
            levels.events = {index: i, level: level};
            
            if (pixelsPerLevel <= this.minPixelsPerLevel * this.pixelAspectRatio) {
                // minMsPerPixel
                break;
            }
        }
        /*if(levels.top.index == levels.labels.index && levels.top.index > 0){
            do{
                levels.top.index --;
                    levels.top.level = window.RulerModel.levels[levels.top.index];
            }while(!levels.top.level.topW && levels.top.index);
        }*/
        return levels;
    };
    window.ScaleManager.prototype.updateLevels = function () {
        //3. Select actual level
        this.levels = this.calcLevels(this.msPerPixel);
    };
    window.ScaleManager.prototype.alignStart = function (level) { // Align start by the grid using level
        return level.interval.alignToPast(this.visibleStart);
    };
    window.ScaleManager.prototype.alignEnd = function (level) { // Align end by the grid using level
        return level.interval.alignToFuture(this.visibleEnd);
    };
    window.ScaleManager.prototype.coordinateToDate = function (coordinate) {
        return Math.round(this.start + coordinate * this.msPerPixel);
    };
    window.ScaleManager.prototype.dateToCoordinate = function (date) {
        return (date - this.start) / this.msPerPixel;
    };
    window.ScaleManager.prototype.dateToScreenCoordinate = function (date, pixelAspectRatio) {
        pixelAspectRatio = pixelAspectRatio || 1;
        return pixelAspectRatio * (this.dateToCoordinate(date) - this.dateToCoordinate(this.visibleStart));
    };
    window.ScaleManager.prototype.screenCoordinateToDate = function (coordinate) {
        return this.coordinateToDate(coordinate + this.dateToCoordinate(this.visibleStart));
    };
// Some function for scroll support
// Actually, scroll is based on some AnchorDate and AnchorPosition (on the screen 0 - 1). And anchor may be 'live', which means live-scrolling
// So there is absolute scroll position in percents
// If we move scroll right or left - we are loosing old anchor and setting new.
// If we are zooming - we are zooming around anchor.
    window.ScaleManager.prototype.getRelativePosition = function () {
        var availableWidth = (this.end - this.start) - (this.visibleEnd - this.visibleStart);
        if (availableWidth === 0) {
            return 0;
        }
        return (this.visibleStart - this.start) / availableWidth;
    };
    window.ScaleManager.prototype.getRelativeWidth = function () {
        return this.msPerPixel * this.viewportWidth / (this.end - this.start);
    };
    window.ScaleManager.prototype.canScroll = function (left) {
        if (left) {
            return this.visibleStart !== this.start;
        }
        
        return this.visibleEnd !== this.end && !((this.watch.playing || this.watch.live) && this.liveMode); // canScroll
    };
    window.ScaleManager.prototype.scrollSlider = function () {
        var relativeWidth = this.getRelativeWidth();
        var scrollBarSliderWidth = Math.max(this.viewportWidth * relativeWidth, this.minScrollBarWidth);
        var scrollingWidth = this.viewportWidth - scrollBarSliderWidth;
        
        var scrollValue = this.getRelativePosition();
        var startCoordinate = this.bound(0, scrollingWidth * scrollValue, scrollingWidth);
        
        return {
            width: scrollBarSliderWidth,
            start: startCoordinate,
            scroll: scrollValue,
            scrollingWidth: scrollingWidth
        };
    };
    window.ScaleManager.prototype.scroll = function (value) {
        /*
            How scrolling actually works:
        
            We need two-way translation between visible dates and scrollbar coordinates.
            Lets look closer at scrollbar scrollSlider.start coordinate on the screen and visibleStart date on the timeline.
            They need to be consistent at any time.
        
            scrollSlider.start belongs to [0, viewportWidth - scrollSlider.width]
            So we can introduce relative coordinate for it:
            relativePosition =  scrollSlider.start / (viewportWidth - scrollSlider.width)
            relativePosition belongs to [0,1]
            
        
            visibleStart belongs to [start, end - visibleWidth]
            visibleWidth = visibleEnd-visibleStart - visible time interval on the timeline
            start - the left date on the timeline (start of the timeline)
            end - the right date on the timeline (current datetime - live position)
        
            So we can introduce relative coordinate for it:
            relativePosition = (visibleStart - start) / ((end - start) - visibleWidth) =
                               (visibleStart - start) / ((end - start) - (visibleEnd - visibleWidth))
        
            relativePosition belongs to [0,1]
        
        
            So we use this relativePosition as a scroll value
        */
        if (typeof (value) === 'undefined') {
            //instead of scrolling by center - we always determine scroll value by left position
            return this.getRelativePosition();
        }
        var anchorDate = this.anchorDate; //Save anchorDate
        var availableWidth = (this.end - this.start) - (this.visibleEnd - this.visibleStart);
        this.setAnchorDateAndPoint(this.start + value * availableWidth, 0); //Move viewport
        this.tryToRestoreAnchorDate(anchorDate); //Try to restore anchorDate
    };
    window.ScaleManager.prototype.getScrollByPixelsTarget = function (pixels) {
        var availableWidth = (this.end - this.start) - (this.visibleEnd - this.visibleStart);
        if (availableWidth < 1) {
            return 0;
        }
        var relativeValue = (this.visibleEnd - this.visibleStart) / availableWidth;
        return this.bound(0, this.scroll() + pixels / this.viewportWidth * relativeValue, 1);
    };
    window.ScaleManager.prototype.getScrollTarget = function (pixels) {
        return this.bound(0, pixels / this.viewportWidth, 1);
    };
    window.ScaleManager.prototype.scrollByPixels = function (pixels) {
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
    
    window.ScaleManager.prototype.zoomToMs = function (zoom) {
        // x(z) = Xmin * e^( ln(Xmax/Xmin) * z) = Xmin * (Xmax/Xmin) ^ z
        var msPerPixel = this.minMsPerPixel * Math.pow(this.absMaxMsPerPixel / this.minMsPerPixel, zoom);
        return this.bound(this.minMsPerPixel, msPerPixel, this.maxMsPerPixel);
    };
    window.ScaleManager.prototype.msToZoom = function (ms) {
        // z(x) = ln(x/Xmin) / ln (Xmax/Xmin)
        return Math.log(ms / this.minMsPerPixel) / Math.log(this.absMaxMsPerPixel / this.minMsPerPixel);
    };
    window.ScaleManager.prototype.targetLevels = function (zoomTarget) {
        zoomTarget = this.boundZoom(zoomTarget);
        var msPerPixel = this.zoomToMs(zoomTarget);
        return this.calcLevels(msPerPixel);
    };
    window.ScaleManager.prototype.checkZoomedOutNow = function () {
        var invisibleInterval = (this.end - this.start) - (this.visibleEnd - this.visibleStart);
        return invisibleInterval <= this.zoomAccuracyMs;
    };
    window.ScaleManager.prototype.checkZoomOut = function (zoomTarget) {
        // Anticipate visible interval after setting zoomTarget
        var msPerPixel = this.zoomToMs(zoomTarget);
        var visibleInterval = msPerPixel * this.viewportWidth;
        var invisibleInterval = (this.end - this.start) - visibleInterval;
        this.disableZoomOut = invisibleInterval <= this.zoomAccuracyMs;
    };
    window.ScaleManager.prototype.checkZoomIn = function (zoomTarget) {
        this.disableZoomIn = zoomTarget <= this.fullZoomInValue();
    };
    window.ScaleManager.prototype.checkZoom = function (zoomTarget) {
        this.checkZoomOut(zoomTarget);
        this.checkZoomIn(zoomTarget);
    };
    window.ScaleManager.prototype.checkZoomAsync = function (zoomTarget) {
        var self = this;
        var result = self.$q.defer();
        setTimeout(function () {
            var oldDisableZoomOut = self.disableZoomOut;
            var oldDisableZoomIn = self.disableZoomIn;
            
            self.checkZoom(zoomTarget);
            
            if (oldDisableZoomOut !== self.disableZoomOut ||
                oldDisableZoomIn !== self.disableZoomIn) {
                result.resolve();
            } else {
                result.reject();
            }
        });
        return result.promise;
    };
    window.ScaleManager.prototype.zoom = function (zoomValue) { // Get or set zoom value (from 0 to 1)
        if (typeof (zoomValue) === 'undefined') {
            return this.msToZoom(this.msPerPixel);
        }
        
        this.msPerPixel = this.zoomToMs(zoomValue);
        
        this.updateCurrentInterval();
    };
    window.ScaleManager.prototype.zoomAroundPoint = function (zoomValue, coordinate) {
        this.setAnchorCoordinate(coordinate); // try to set new anchorDate
        this.zoom(zoomValue);                    // zoom and update visible interval
    };
    window.ScaleManager.prototype.lastMinute = function () {
        return this.end - this.lastMinuteInterval;
    };
})();
