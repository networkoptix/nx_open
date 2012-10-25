function pageOffset(win)
{
    if(!win) win = window;
    var pos = {left:0,top:0};

    if(typeof win.pageXOffset != 'undefined')
    {
         // Mozilla/Netscape
         pos.left = win.pageXOffset;
         pos.top = win.pageYOffset;
    }
    else
    {
         var obj = (win.document.compatMode && win.document.compatMode == "CSS1Compat") ?
         win.document.documentElement : win.document.body || null;

         pos.left = obj.scrollLeft;
         pos.top = obj.scrollTop;
    }
    return pos;
}

   function doResize() { 
     var clheight, headheight;
     if (self.innerHeight) // all except Explorer 
     { clheight = self.innerHeight; } 
     else if (document.documentElement && document.documentElement.clientHeight) // Explorer 6 Strict Mode 
     { clheight = document.documentElement.clientHeight; } 
     else if (document.body) // other Explorers 
     { clheight = document.body.clientHeight; } 
     headheight = document.getElementById('idheader').clientHeight;
     if (clheight < headheight ) {clheight = headheight + 1;}
     document.getElementById('idcontent').style.height = clheight - document.getElementById('idheader').clientHeight +'px'; 
   } 

   function nsrInit() { 
     contentbody = document.getElementById('idcontent'); 
     if (contentbody) { 
       aTop = pageOffset.top; //document.getElementById('body').scrollTop; 
       contentbody.className = 'nonscroll'; 
       document.getElementsByTagName('body')[0].className = 'nonscroll'; 
       document.getElementsByTagName('html')[0].className = 'nonscroll'; 
       window.onresize = doResize; 
       doResize(); 
       if (contentbody.scrollTo) { contentbody.scrollTo(aTop,0); }
      } 
   } 

