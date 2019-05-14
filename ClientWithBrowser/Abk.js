var xhrSend = null;
xhrSend = new XMLHttpRequest();
var g_nSessionId = 0; // session id
var g_strClientAddress = ""; // client IP address, got from server
var g_fnOnEvent; // callback when event occured


//--------------------------------------------------------------------------
// AbkEventPoller is used to poll events with the long polling methods
//
// to use it, create an instance, e.g. var poller = new AbkEventPoller(OnLoggerEvent)


function AbkEventPoller(fnOnEvent)
{
  g_fnOnEvent = fnOnEvent;
  xhrRecieve = new XMLHttpRequest();
  AbkEventPoller.RequestInfo(); // start long polling
}

AbkEventPoller.ProcessEventResponse = function ()
{
  if (AbkEventPoller.xhrRecieve.readyState == 4 && AbkEventPoller.xhrRecieve.status == 200)
  {
    var objEvent = JSON.parse(AbkEventPoller.xhrRecieve.responseText);
    g_fnOnEvent(objEvent);
    AbkEventPoller.RequestInfo(); // next long polling cycle
  }
}

AbkEventPoller.RequestInfo = function ()
{
  AbkEventPoller.xhrRecieve.onreadystatechange = this.ProcessEventResponse;
  AbkEventPoller.xhrRecieve.open("GET", "/abk/events/server_event?SessionId=" + g_nSessionId, true);
  AbkEventPoller.xhrRecieve.setRequestHeader("If-Modified-Since", new Date(0));
  AbkEventPoller.xhrRecieve.send(null);
}

AbkEventPoller.xhrRecieve = new XMLHttpRequest();





//**************************************************************************
// HTTP request abstraction
//
//**************************************************************************

//--------------------------------------------------------------------------
// AbkHttpComposeUri() composes a uri string
// ---------------
// Input: strUri = location string
//        objQuery = query object. each member will be set into the query string
// Return: the complete uri string

function AbkHttpComposeUri(strUri, objQuery)
{
  var strUriComposed = strUri + "?SessionId=" + g_nSessionId;
  if (objQuery)
  {
    for (var item in objQuery)
    {
      strUriComposed += "&" + item + "=" + objQuery[item];
    }
  }
  return strUriComposed;
}


//--------------------------------------------------------------------------
// AbkHttpPut() sends an http put request (synchronousely)
// --------
// Input: strUri = location string
//        objQuery = object put to the query string
//        objSend = object to be sent in the body of the request
// Return: -

function AbkHttpPut(strUri, objQuery, objSend)
{
  xhrSend.open("PUT", AbkHttpComposeUri(strUri, objQuery), false);
  xhrSend.send(JSON.stringify(objSend));
}


//--------------------------------------------------------------------------
// AbkHttpPost() sends an http post request (synchronousely)
// ---------
// Input: strUri = location string
//        objSend = object put to the request body
// Return: parsed response as an object

function AbkHttpPost(strUri, objSend)
{
  xhrSend.open("POST", AbkHttpComposeUri(strUri, null), false);
  xhrSend.setRequestHeader("If-Modified-Since", new Date(0));
  xhrSend.send(JSON.stringify(objSend));
  if (xhrSend.readyState == 4 && xhrSend.status == 200)
  {
    return JSON.parse(xhrSend.responseText);    // eval("(" + xhrSend.responseText + ")");
  }
  return null
}


//--------------------------------------------------------------------------
// AbkHttpGet() sends an http get request (synchronousely)
// ------------
// Input: strUri = location string
//        objQuery = object put to the query string
// Return: parsed response as an object

function AbkHttpGet(strUri, objQuery)
{
  xhrSend.open("GET", AbkHttpComposeUri(strUri, objQuery), false);
  xhrSend.setRequestHeader("If-Modified-Since", new Date(0));
  xhrSend.send(); // JSON.stringify(objSend));
  if (xhrSend.readyState == 4 && xhrSend.status == 200)
  {
    return JSON.parse(xhrSend.responseText);    // eval("(" + xhrSend.responseText + ")");
  }
  return null
}


//--------------------------------------------------------------------------
// AbkHttpGetRaw() sends an http get request (synchronousely)
// ---------------
// Input: strUri = location string
// Return: read file as string

function AbkHttpGetRaw(strUri)
{
  xhrSend.open("GET", strUri, false);
  xhrSend.setRequestHeader("If-Modified-Since", new Date(0));
  xhrSend.send();
  if (xhrSend.readyState == 4 && xhrSend.status == 200)
  {
    return xhrSend.responseText;
  }
  return null
}

//--------------------------------------------------------------------------
// AbkHttpDelete() sends an http delete request (synchronousely)
// -----------
// Input: strUri = location string
//        objQuery = object put to the query string
//        objSend = object put to the request body
// Return: -

function AbkHttpDelete(strUri, objQuery, objSend)
{
  xhrSend.open("DELETE", AbkHttpComposeUri(strUri, objQuery), false);
  xhrSend.setRequestHeader("If-Modified-Since", new Date(0));
  xhrSend.send(JSON.stringify(objSend));
}


//--------------------------------------------------------------------------
// AbkGetLastModified() returns the last-modified date of a file
// --------------------
// Input: strUrl = location string
// Return: in case of success: last-modified-date of the file
//         in case of an error: null

function AbkGetLastModified(strUrl)
{
  xhrSend.open("HEAD", strUrl, false);
  xhrSend.setRequestHeader("If-Modified-Since", new Date(0));
  xhrSend.send();
  if (xhrSend.readyState == 4 && xhrSend.status == 200)
  {
    var strRet = xhrSend.getResponseHeader("Last-Modified");
    var dtModified = new Date(Date.parse(strRet));   //  AbkParseDate(strRet);
    var i = 12;
    return dtModified
  }
  return null
}



//--------------------------------------------------------------------------
// AbkParseDate() parses a string with date information
// Iput: strDate = date to be parsed
// Return: date object or unchanged string in case of error
// see also http://blog.activa.be/index.php/2010/03/handling-dates-in-json-responses-with-jquery-1-4-the-easy-way/

function AbkParseDate(strDate)
{
  var a;
  if (typeof strDate === 'string')
  {
    if (strDate.substring(0, 2) === "\\\"")
    {
      strDate = strDate.substring(2, strDate.length - 2);
    }
    else if (strDate.substring(0, 1) === "\"")
    {
      strDate = strDate.substring(1, strDate.length - 1);
    }
    else if (strDate.substring(0, 1) === "\\")
    {
      strDate = strDate.substring(1, strDate.length - 1);
    }
    a = /^(\d{4})-(\d{2})-(\d{2})T(\d{2}):(\d{2}):(\d{2}(?:\.\d*)?)Z$/.exec(strDate);
    if (a)
    {
      return new Date(Date.UTC(+a[1], +a[2] - 1, +a[3], +a[4], +a[5], +a[6]));
    }
    if (strDate.slice(0, 5) === 'Date(' && strDate.slice(-1) === ')')
    {
      var d = new Date(strDate.slice(5, -1));
      if (d)
      {
        return d;
      }
    }
  }
  return strDate;
}




//**************************************************************************
// HighLevel functions for access to server
//
//**************************************************************************


//--------------------------------------------------------------------------
// AbkGetSession() gets a session id and stores it globally
// ---------------
// Input: strClientClass = class of the client, e.g. "Display"
//        strClientType = type of client, e.g. "Mytronics_superdisplay3000"
//        strClientSerial = serial number of client. may be null, if no serial number
//                          is available due to a single-per-class system (e.g. single-display-system)
// Return: none. Session id will be stored globally

function AbkGetSession(strClientClass, strClientType,strClientSerial)
{
  var objSend= {"Class":strClientClass, "Type":strClientType}; // setup the mandatory part
  if(strClientSerial)
    objSend.Serial=strClientSerial;
  var objResponse = AbkHttpPost("/abk/system_information/session_id", objSend); // generate session at server side
  g_nSessionId = objResponse.SessionId;
}


//--------------------------------------------------------------------------
// AbkSendEvent() composes a uri string
// ---------------
// Input: strEventType = event type string
//        strStringParam = string parameter to be sent with the event
//        dParam1, dParam2 = numeric parameters
//        bPrivate: if true, client intends to process the reflected event
//                  by itself and the server shall not reflect the event
//                  to other clients
// Return: -

function AbkSendEvent(strEventType, strStringParam, dParam1, dParam2, bPrivate)
{
  var strTime = JSON.stringify(new Date());
  AbkHttpPut("/abk/events/client_event", null, { Sender: g_nSessionId, Time: strTime, EventType: strEventType, StringParam: strStringParam, Param1: dParam1, Param2: dParam2, Private: bPrivate });
}


//--------------------------------------------------------------------------
function AbkDeleteSession()
{
  AbkHttpDelete("/abk/system_information/session_id");
}


//--------------------------------------------------------------------------
function AbkGetClientAddress()
{
  var objResponse = AbkHttpGet("/abk/system_information/client_address");
  g_strClientAddress = objResponse.Address;
}


//--------------------------------------------------------------------------
function AbkGetVarValue(strVarName)
{
  var objResponse = AbkHttpPost("/abk/variables/var_value", { GetList: [strVarName] });
  return objResponse.Data[0];
}

//--------------------------------------------------------------------------
function AbkSetVarValue(strVarName, newValue)
{
  AbkHttpPut("/abk/variables/var_value", null, { PutList: [{ Name: strVarName, Value: newValue}] });
}

//--------------------------------------------------------------------------
function AbkGetMailboxValue(strVarName)
{
  var objResponse = AbkHttpPost("/abk/variables/mailbox_value", { GetList: [strVarName] });
  return objResponse.Data[0];
}

//--------------------------------------------------------------------------
function AbkSetMailboxValue(strVarName, newValue)
{
  AbkHttpPut("/abk/variables/mailbox_value", null, { PutList: [{ Name: strVarName, Value: newValue}] });
}

function AbkGetVarMetaSingleVar(strVarName)
{
  var objMeta = AbkHttpPost("/abk/variables/var_meta", { VarList: [strVarName] });
  AbkMetaRawToPhysical(objMeta.MetaData[0]);
  return objMeta.MetaData[0]; // return the first (is the one and only member) meta data
}

function AbkGetVarMetaMultipleVars(arryVarList)
{
  var objMeta = AbkHttpPost("/abk/variables/var_meta", { VarList: arryVarList });
  for (var meta in objMeta.MetaData)
  {
    AbkMetaRawToPhysical(objMeta.MetaData[meta]);
  }
  return objMeta.MetaData; // return the array contining objects for each variable with the meta data
}

function AbkMetaRawToPhysical(objMeta)
{
  if (objMeta.Factor && objMeta.Offset)
  {
    if (objMeta.RangeMin)
      objMeta.RangeMin = objMeta.RangeMin * objMeta.Factor + objMeta.Offset;
    if (objMeta.RangeMax)
      objMeta.RangeMax = objMeta.RangeMax * objMeta.Factor + objMeta.Offset;
    for (var nThreshold = 0; nThreshold < objMeta.Thresholds.length;nThreshold++ )
      objMeta.Thresholds[nThreshold] = objMeta.Thresholds[nThreshold] * objMeta.Factor + objMeta.Offset;
  }
}

function AbkSetupDaqList(strDaqName, nCycleMs, arryVars)
{
  var objDaqList = { Name: strDaqName };
  if (nCycleMs)
    objDaqList.Cycle = nCycleMs;
  if (arryVars)
    objDaqList.DaqList = arryVars;
  AbkHttpPut("/abk/variables/daq_list", null, objDaqList);
}

function AbkSetupDaqTrend(strDaqName, nCycleMs, strVarName)
{
  var objDaqTrend = { Name: strDaqName };
  if (nCycleMs)
    objDaqTrend.Cycle = nCycleMs;
  objDaqTrend.VarName = strVarName;
  AbkHttpPut("/abk/variables/daq_trend", null, objDaqTrend);
}

function AbkSetDaqUpdateCycle(strDaqListName, nCycleMs)
{
  AbkHttpPut("/abk/variables/daq_list", null, { Name: strDaqListName, Cycle: nCycleMs });
}

function AbkGetServerTime()
{
  var tmServer = AbkHttpGet("/abk/system_information/current_time");
  return AbkParseDate(tmServer.Time);
}


function AbkPutClientState(strClass, strType, strSerial, strWrite)
{
  var strUlr = "/abk/client_states/" + strClass + "_" + strType + "_" + strSerial + ".txt";
  xhrSend.open("PUT", strUlr, false);
  xhrSend.send(strWrite);
}

function AbkGetClientState(strClass, strType, strSerial)
{
  var strUlr = "/abk/client_states/" + strClass + "_" + strType + "_" + strSerial + ".txt";
  xhrSend.open("GET", strUlr, false);
  xhrSend.setRequestHeader("If-Modified-Since", new Date(0));
  xhrSend.send();
  if (xhrSend.readyState == 4 && xhrSend.status == 200)
  {
    return xhrSend.responseText;
  }
  return null
}


function AbkGetFormAsHtml(strFormName, strEvalOnButton)
{
  var objForm = AbkHttpGet("/abk/forms/" + strFormName);
  var strInnerHtml;
  strInnerHtml = "<h2>" + objForm.Caption + "</h2><br /><br />";
  //strInnerHtml = "<form>";
  strInnerHtml += "<table id='formtbl_"+strFormName+"' border='0' cellpadding='5' cellspacing='0'>";
  for (var nItem in objForm.Controls)
  {
    strInnerHtml += "<tr>";
    var objControl = objForm.Controls[nItem]
    var strControlName = objControl.Name;
    var strInitialValue=null;
    if (objControl.InitialValue)
      strInitialValue = objControl.InitialValue;
    var bReadOnly = false;
    if (objControl.ReadOnly)
      bReadOnly = objControl.ReadOnly;
    switch (objControl.Type)
    {
      case "Input":
        var strInputType = "text";
        if (objControl.Password && objControl.Password == true)
          strInputType = "password";
        strInnerHtml += "<td>" + objControl.Caption + ": </td>";
        strInnerHtml += "<td>";
        if (strInitialValue == null)
          strInitialValue = "";
        strInnerHtml += "<input name='" + strControlName + "' value='" + strInitialValue + "' type='" + strInputType + "' size='30'";
        if (objControl.MaxLen)
          strInnerHtml += " maxlength='" + objControl.MaxLen + "'";
        if (bReadOnly == true)
          strInnerHtml += " readonly";
        strInnerHtml += "/>";
        strInnerHtml += "</td>";
        break;
      case "Checkbox":
        strInnerHtml += "<td>" + objControl.Caption + ": </td>";
        strInnerHtml += "<td>";
        var strCheckReadOnly = "";
        // if (bReadOnly)
          strCheckReadOnly = "readonly";
        strInitialValue = objControl.InitialValue ? "checked" : "";
        strInnerHtml += "<input type='checkbox' name='" + strControlName + "' " + strInitialValue + " "+strCheckReadOnly+" />"
        strInnerHtml += "</td>";
        break;
      case "List":
        strInnerHtml += "<td>" + objControl.Caption + ": </td>";
        strInnerHtml += "<td>";
        strInnerHtml += "<select size='1' style='width: 200px' name='" + strControlName + "'>";
        for (var nOption in objControl.Options)
        {
          strInnerHtml += "<option>" + objControl.Options[nOption] + "</option>";
        }
        strInnerHtml += "</select>";
        strInnerHtml += "</td>";
        break;
      case "Button":
        var strButtonType = "button";
        if (objControl.Submit)
          strButtonType = "submit";
        else if (objControl.Cancel)
          strButtonType = "reset";
        strInnerHtml += "<td></td><td>";
        strInnerHtml += "<input name='" + strControlName + "' type='"+strButtonType+"' value='" + objControl.Caption + "' onclick='" + strEvalOnButton + "(\"" + strFormName + "\",\"" + strControlName + "\")' />";
        strInnerHtml += "</td>";
        break;
    }
    strInnerHtml += "</tr>";
  }
  strInnerHtml += "</table>";
  //strInnerHtml = "</form>";
  return strInnerHtml;
}


function AbkSendForm(strFormName, strFireButtonName)
{
  var bCloseRequired = false;
  var objReturnData = new Object();
  var tblControls = document.getElementById("formtbl_"+strFormName);
  for (var nControl = 0; nControl < tblControls.rows.length; nControl++)
  {
    var cellControl = tblControls.rows[nControl].cells[1];
    var ctrlX = cellControl.firstElementChild;
    var value = ctrlX.value;
    var strName = ctrlX.name;
    switch (ctrlX.type)
    {
      case "text":
        break;
      case "checkbox":
        value=ctrlX.checked;
        break;
      case "select-one":
        value=ctrlX.options.selectedIndex;
        break;
      case "submit":
      case "cancel":
        bCloseRequired = true;
      case "button":
        if (strName == strFireButtonName)
          value = true;
        else
          value = false;
        break;
    }
    objReturnData[strName] = value;
  }
  AbkHttpPut("/abk/forms/" + strFormName, null, objReturnData);
  return bCloseRequired;
}

