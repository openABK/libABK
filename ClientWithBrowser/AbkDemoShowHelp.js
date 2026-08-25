
// see also http://stackoverflow.com/questions/4666367/how-do-i-position-a-div-relative-to-the-mouse-pointer-using-jquery
var nShowHelpcX = 0; var nShowHelpcY = 0; var nShowHelprX = 0; var nShowHelprY = 0;
function UpdateCursorPosition(e)
{
  nShowHelpcX = e.pageX; nShowHelpcY = e.pageY;
}
function UpdateCursorPositionDocAll(e)
{
  nShowHelpcX = event.clientX; nShowHelpcY = event.clientY;
}
if (document.all) { document.onmousemove = UpdateCursorPositionDocAll; }
else { document.onmousemove = UpdateCursorPosition; }
function AssignPosition(d)
{
  if (self.pageYOffset)
  {
    nShowHelprX = self.pageXOffset;
    nShowHelprY = self.pageYOffset;
  }
  else if (document.documentElement && document.documentElement.scrollTop)
  {
    nShowHelprX = document.documentElement.scrollLeft;
    nShowHelprY = document.documentElement.scrollTop;
  }
  else if (document.body)
  {
    nShowHelprX = document.body.scrollLeft;
    nShowHelprY = document.body.scrollTop;
  }
  if (document.all)
  {
    nShowHelpcX += nShowHelprX;
    nShowHelpcY += nShowHelprY;
  }
  d.style.left = (nShowHelpcX + 10) + "px";
  d.style.top = (nShowHelpcY + 10) + "px";
}


function HideHelp(d)
{
  var divBubble = document.getElementById("helpbubble1");
  divBubble.style.display = "none";
  divBubble.innerHTML = "";
}


function ShowHelp(strUrl)
{
  var divBubble = document.getElementById("helpbubble1");
  AssignPosition(divBubble);
  divBubble.style.display = "block";
  divBubble.innerHTML = AbkHttpGetRaw("AbkDemoHelp/"+ strUrl);

}


function ReverseContentDisplay(d)
{
  if (d.length < 1) { return; }
  var dd = document.getElementById(d);
  AssignPosition(dd);
  if (dd.style.display == "none") { dd.style.display = "block"; }
  else { dd.style.display = "none"; }
}
