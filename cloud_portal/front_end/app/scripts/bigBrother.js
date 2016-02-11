'use strict';

var activatedEye = false;
document.addEventListener("mousemove", function (event) {
    var $bb = $(".big-brother");
    var $pupil = $(".big-brother>.eye .pupil");
    var maxLength = 150;
    var activationLegth = 60;

    var eyeBorder = 6;
    var eyeSize = $bb.width();
    var pupilBasePosition = 7.5;

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

    if(length < activationLegth){
        activatedEye = true;
    }
    if(!activatedEye){
        return;
    }

    var pupilFloatX = 35;
    var pupilFloatY = 20;
    var proportionLengthX = pupilFloatX/Math.max(maxLength,length);
    var proportionLengthY = pupilFloatY/Math.max(maxLength,length);
    var pupilPosition = {
        top:  pupilBasePosition + proportionLengthY * direction.top + "px",
        left: pupilBasePosition + proportionLengthX * direction.left + "px"
    };
    $pupil.css(pupilPosition);
});