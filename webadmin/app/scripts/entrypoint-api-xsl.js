$(function () {
    $(".nav .glyphicon").click(function(){
        var $container = $(this).parent().parent();

        if($container.hasClass("active")){
            $container.removeClass("pinned")
                    .toggleClass("pinned-hidden");
        }else{
            $container.toggleClass("pinned")
                    .removeClass("pinned-hidden");
        }
        return false;
    });
});