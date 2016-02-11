'use strict';

document.addEventListener("mousemove", function (event) {
    var $bb = $(".big-brother");
    var $pupil = $(".big-brother>.eye .pupil");
    var maxLength = 150;

    var eyeBorder = 6;
    var eyeSize = $bb.width();
    var pupilBasePosition = eyeSize/10 + eyeBorder/4;

    var bbOffset = $bb.offset();
    if(!bbOffset){
        return;
    }
    var bbCenter = {
        left:bbOffset.left + eyeSize/2 + eyeBorder,
        top:bbOffset.top + eyeSize/2 + eyeBorder
    };
    var direction = {
        left:event.pageX - bbCenter.left,
        top:event.pageY - bbCenter.top
    };

    var length = Math.sqrt(direction.left*direction.left + direction.top*direction.top);

    if(length> maxLength){
        return;
    }
    var pupilFloat = 20;
    var proportionLength = pupilFloat/Math.max(maxLength,length);
    var pupilPosition = {
        top:pupilBasePosition + proportionLength*direction.top + "px",
        left:pupilBasePosition + proportionLength*direction.left + "px",
    }
    $pupil.css(pupilPosition);
});