'use strict';

function Chunk(start,end,level){
    this.start = start;
    this.end = end;
    this.level = level;
    this.children = [];
}

function CameraRecordsProvider(cameras){
    this.cameras = cameras;
    this.chunksTree  = null;

    var self = this;
    //1. request first detailization to get initial bounds
    this.setInterval(0,(new Date()).getTime(), 0).then(function(){
        // Depends on this interval - choose minimum interval, which contains all records and request deeper detailization

        var searchdetailization = self.now() - self.chunksTree.start;
        var targetLevel = _.find(CameraRecordsProvider.levels, function(level){
            return level.detailization < searchdetailization;
        }) ;

        var nextLevel = typeof(targetLevel)!=='undefined' ? CameraRecordsProvider.levels.indexOf(targetLevel) : CameraRecordsProvider.levels.length-1;
        // request better detailization
        self.setInterval(self.chunksTree.start, self.now(), nextLevel);
    });

    //2. getCameraHistory
}

CameraRecordsProvider.prototype.now = function(){
    return (new Date()).getTime();
};

/**
 * Presets for detailization levels
 * @type {{detailization: number}[]}
 */
CameraRecordsProvider.levels = [
    {detailization:1000*60*60*24*365*100},//centuries - whole interval.
    {detailization:1000*60*60*24*365},//years
    {detailization:1000*60*60*24*30},//months
    {detailization:1000*60*60*24},//days
    {detailization:1000*60*60},//hours
    {detailization:1000*60},//minutes
    {detailization:1000}//seconds
];

/**
 * Request records for interval, add it to cache, update visibility splice
 *
 * @param start
 * @param end
 * @param level
 * @return Promise
 */
CameraRecordsProvider.prototype.setInterval = function (start,end,level){

    this.start = start;
    this.end = end;
    this.level = level;

    var self = this;
    //1. Request records for interval
    mediaserver.getRecords('/',this.cameras[0],start,end,CameraRecordsProvider.levels[level].detailization)
        .then(function(data){

            for(var i = 0; i <data.length;i++){
                self.addChunk(new Chunk(data[i].start,data[i].end,level));
            }
        });

    //2. Splice cache for existing interval
    //3. Store it as local cache
};

/**
 * Add chunk to tree - find or create good position for it
 * @param chunk
 * @param parent - parent for recursive call
 */
CameraRecordsProvider.prototype.addChunk = function(chunk, parent){
    parent = parent || this.chunksTree;
    // Go through tree, find good place for chunk

    if(parent.children.length == 0){ // no children yet
        if(parent.level === chunk.level + 1){ //
            parent.children.push(chunk);
            return;
        }
        parent.children.push(new Chunk(chunk.start,chunk.and,parent.level+1));
        this.addChunk(chunk,parent.children[0]);
    }else {
        for (var i = 0; i < parent.children.length; i++) {
            var currentChunk = parent.children[i];
            if (currentChunk.end < chunk.start) {
                continue;
            }
            if (currentChunk.level == chunk.level) {
                //we are on right position - place chunk before current
                parent.children.splice(i, 0, chunk);
            } else {
                //Here we should go deeper
                this.addChunk(chunk, currentChunk);
            }
            return;
        }
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
CameraRecordsProvider.prototype.splice = function(start, end, level, parent, result){
    parent = parent || this.chunksTree;
    result = result || [];

    for(var i = 0 ; i < parent.children.length ; i ++ ){
        var currentChunk = parent.children[i];
        if(currentChunk.end <= start || currentChunk.start>=end || currentChunk.level == level){
            result.push(currentChunk);
            continue;
        }

        this.splice(start, end, level, currentChunk, result);
    }

    return result;
};

angular.module('webadminApp')
    .factory('cameraRecords', function (mediaserver) {

    });