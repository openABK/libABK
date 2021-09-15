/**
@file abk.js openABK helping and abstraction functions
@author Dietmar Burger - https://www.openABK.org
*/



var xhrSend = new XMLHttpRequest(); ///< XML http request used to send data and events
var g_nSessionId = 0; ///< ID of the sesstion. This ID gets assigned from the server



/** Tests whether the answer of the request was successfully received
@param xhrTest request object to be tested for success
@return true, if the anwer was successfully received, false otherwise
*/
function AbkXhrAnswerSuccess (xhrTest)
{
  return (xhrTest.readyState == 4) && (xhrTest.status == 200);
}




/** Sets the request header which suppresses cacheing
@param xhrSet request the header shall be set for
*/
function AbkSetRequestHeaderNoCacheing (xhrSet)
{
  xhrSet.setRequestHeader("If-Modified-Since", new Date(0));
}





/** AbkEventPoller is used to poll events with the long polling methods
@note To use it, create an instance, e.g. var poller = new AbkEventPoller(OnLoggerEvent)
*/
class AbkEventPoller
{

  /** Constructor
  @param fnOnEvent Function to be called when the server responds to the request (the response can be seen as an event)
  */
  constructor(fnOnEvent)
  {
    this.fnOnEvent = fnOnEvent;
    this.xhrLongpoll = new XMLHttpRequest();
    this.RequestNext(); // start long polling
  }

  /** Sends the request for the next long polling cycle.
   When the answer of the request arrives, the next request is issued automatically
  */
  RequestNext ()
  {
    var self = this;
    this.xhrLongpoll.onreadystatechange = function ()
    {
      if (AbkXhrAnswerSuccess(self.xhrLongpoll))
      {
        var objEvent = JSON.parse(self.xhrLongpoll.responseText);
        self.fnOnEvent(objEvent);
        self.RequestNext(); // next long polling cycle
      }
    };
    this.xhrLongpoll.open("GET", "/abk/events/server_event?SessionId=" + g_nSessionId, true);
    AbkSetRequestHeaderNoCacheing(this.xhrLongpoll);
    this.xhrLongpoll.send(null);
  }

}






/** composes an URI string
@param strUrl Location string
@param objQuery query object. each member will be set into the query string
@return The complete URI string
 */
function AbkHttpComposeUri (strUrl, objQuery)
{
  var strUriComposed = strUrl + "?SessionId=" + g_nSessionId;
  if (objQuery)
  {
    for (var item in objQuery)
    {
      strUriComposed += "&" + item + "=" + objQuery[item];
    }
  }
  return strUriComposed;
}




/** sends an http put request (synchronousely)
@param strUrl Location URL string without query parameters
@param objQuery object to be put to the query string
@param objSend object to be sent in the body of the request
*/
function AbkHttpPut (strUrl, objQuery, objSend)
{
  xhrSend.open("PUT", AbkHttpComposeUri(strUrl, objQuery), false);
  xhrSend.send(JSON.stringify(objSend));
}




/** sends an http post request (synchronousely)
@param strUrl Location URL string without query parameters
@param objSend object to be sent in the body of the request
@return  parsed response as an object or null if failed
*/
function AbkHttpPost (strUrl, objSend)
{
  xhrSend.open("POST", AbkHttpComposeUri(strUrl, null), false);
  AbkSetRequestHeaderNoCacheing(xhrSend);
  xhrSend.send(JSON.stringify(objSend));
  if (AbkXhrAnswerSuccess(xhrSend))
  {
    return JSON.parse(xhrSend.responseText);    // eval("(" + xhrSend.responseText + ")");
  }
  return null
}




/** sends an http get request without adding query string and without decoding the result (synchronousely)
@param strUri location string
@return read file as string
*/
function AbkHttpGetRaw (strUri)
{
  xhrSend.open("GET", strUri, false);
  AbkSetRequestHeaderNoCacheing(xhrSend);
  xhrSend.send();
  if (AbkXhrAnswerSuccess(xhrSend))
  {
    return xhrSend.responseText;
  }
  return null
}




/** sends an http get request (synchronousely)
@param strUrl Location URL string without query parameters
@param objQuery object to be put to the query string
@return parsed response as an object
*/
function AbkHttpGet (strUrl, objQuery)
{
  xhrSend.open("GET", AbkHttpComposeUri(strUrl, objQuery), false);
  AbkSetRequestHeaderNoCacheing(xhrSend);
  xhrSend.send();
  if (AbkXhrAnswerSuccess(xhrSend))
  {
    return JSON.parse(xhrSend.responseText);    // eval("(" + xhrSend.responseText + ")");
  }
  return null
}



/** sends an http delete request (synchronousely)
@param strUrl location string
@param objQuery object put to the query string
@param objSend object put to the request body
*/
function AbkHttpDelete (strUrl, objQuery, objSend)
{
  xhrSend.open("DELETE", AbkHttpComposeUri(strUrl, objQuery), false);
  AbkSetRequestHeaderNoCacheing(xhrSend);
  xhrSend.send(JSON.stringify(objSend));
}




/** returns the last-modified date of a file
@param strUrl location string
@return In case of success: last-modified-date of the file. In case of an error: null
*/
function AbkGetLastModified (strUrl)
{
  xhrSend.open("HEAD", strUrl, false);
  AbkSetRequestHeaderNoCacheing(xhrSend);
  xhrSend.send();
  if (AbkXhrAnswerSuccess(xhrSend))
  {
    var strRet = xhrSend.getResponseHeader("Last-Modified");
    var dtModified = new Date(Date.parse(strRet));   //  AbkParseDate(strRet);
    return dtModified
  }
  return null
}




/** parses a string with date information
@note: We expect data being in this format:
 a) \"2021-01-01T23:28:56.782Z or 
 b) \2021-01-01T23:28:56.782Z or 
 c) "2021-01-01T23:28:56.782Z or 
 d) Date(...)
@param strDate Date to be parsed
@return Date object or unchanged string in case of error
@note See also http://blog.activa.be/index.php/2010/03/handling-dates-in-json-responses-with-jquery-1-4-the-easy-way/
*/
function AbkParseDate (strDate)
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




/** Requets a session id and stores it globally
@param strClientClass Class of the client, e.g. "Display"
@param strClientType type of client, e.g. "Mytronics_superdisplay3000"
@param strClientSerial serial number of client. may be null, if no serial number is available due to a single-per-class system (e.g. single-display-system)
*/
function AbkGetSession (strClientClass, strClientType, strClientSerial)
{
  var objSend = { "Class": strClientClass, "Type": strClientType }; // setup the mandatory part
  if (strClientSerial)
    objSend.Serial = strClientSerial;
  var objResponse = AbkHttpPost("/abk/system_information/session_id", objSend); // generate session at server side
  g_nSessionId = objResponse.SessionId;
}




/** Sends a client-event to the server
@param strEventType event type string
@param strStringParam string parameter to be sent with the event
@param dParam1 1st numeric parameter
@param dParam2 2nd numeric parameter
@param bPrivate: if true, client intends to process the reflected event by itself and the server shall not reflect the event to other clients
*/
function AbkSendEvent (strEventType, strStringParam, dParam1, dParam2, bPrivate)
{
  var strTime = JSON.stringify(new Date());
  AbkHttpPut("/abk/events/client_event", null, { Sender: g_nSessionId, Time: strTime, EventType: strEventType, StringParam: strStringParam, Param1: dParam1, Param2: dParam2, Private: bPrivate });
}




/** Deletes the current session at the server
*/
function AbkDeleteSession ()
{
  AbkHttpDelete("/abk/system_information/session_id");
  g_nSessionId = 0;
}




/** Queries the client address (of this device) as the server sees me
@return My address from the servers side, null on error
*/
function AbkGetClientAddress ()
{
  var objResponse = AbkHttpGet("/abk/system_information/client_address");
  if (objResponse != null)
    return objResponse.Address;
  else
    return null;
}




/** Queries the current value of a variable
@param strVarName Name of the variable to be queried
@return the value of the variable or null on failure
*/
function AbkGetVarValue (strVarName)
{
  var objResponse = AbkHttpPost("/abk/variables/var_value", { GetList: [strVarName] });
  if (objResponse != null)
    return objResponse.Data[0];
  else
    return null;
}




/** Sets the value of a variable in the logger
@param strVarName Name of the variable to be set
@param newValue Value for the variable
*/
function AbkSetVarValue (strVarName, newValue)
{
  AbkHttpPut("/abk/variables/var_value", null, { PutList: [{ Name: strVarName, Value: newValue }] });
}




/** Queries the current value of a mailbox
@param strVarName Name of the mailbox to be queried
@return the value of the mailbox or null on failure
*/
function AbkGetMailboxValue (strVarName)
{
  var objResponse = AbkHttpPost("/abk/variables/mailbox_value", { GetList: [strVarName] });
  if (objResponse != null)
    return objResponse.Data[0];
  else
    return null;
}




/** Sets the value of a mailbox in the logger
@param strVarName Name of the mailbox to be set
@param newValue Value for the mailbox
*/
function AbkSetMailboxValue (strVarName, newValue)
{
  AbkHttpPut("/abk/variables/mailbox_value", null, { PutList: [{ Name: strVarName, Value: newValue }] });
}



/** Queries the meta-data of one variable
@param strVarName Name of the variable to be queried
@return The meta-data of the variable
*/
function AbkGetVarMetaSingleVar (strVarName)
{
  var objMeta = AbkHttpPost("/abk/variables/var_meta", { VarList: [strVarName] });
  AbkMetaRawToPhysical(objMeta.MetaData[0]);
  return objMeta.MetaData[0]; // return the first (is the one and only member) meta data
}




/** Queries the meta-data of multiple variables
@param arryVarList Array of strings of variable names
@return Array with the meta-data of the variables in the same order as in arryVarList
*/
function AbkGetVarMetaMultipleVars (arryVarList)
{
  var objMeta = AbkHttpPost("/abk/variables/var_meta", { VarList: arryVarList });
  for (var meta in objMeta.MetaData)
  {
    AbkMetaRawToPhysical(objMeta.MetaData[meta]);
  }
  return objMeta.MetaData;
}




/** Converts the raw meta-data (usually in the sensor-related units) to physical meta-data
@param objMeta Object with the meta-data of a variable
*/
function AbkMetaRawToPhysical (objMeta)
{
  if (objMeta.Factor && objMeta.Offset)
  {
    if (objMeta.RangeMin)
      objMeta.RangeMin = objMeta.RangeMin * objMeta.Factor + objMeta.Offset;
    if (objMeta.RangeMax)
      objMeta.RangeMax = objMeta.RangeMax * objMeta.Factor + objMeta.Offset;
    for (var nThreshold = 0; nThreshold < objMeta.Thresholds.length; nThreshold++)
      objMeta.Thresholds[nThreshold] = objMeta.Thresholds[nThreshold] * objMeta.Factor + objMeta.Offset;
  }
}



/** Installs a DAQ (data acquisition) list at the server
@note A DAQ list defines a list of variables, whose values are transferred at once for each DAQ cycle 
@param strDaqName Name for the DAQ list
@param nCycleMs Polling cycle when reading data with long-polling
@param arryVars array of names of the variables to be put to the daq list
*/
function AbkSetupDaqList (strDaqName, nCycleMs, arryVars)
{
  var objDaqList = { Name: strDaqName };
  if (nCycleMs)
    objDaqList.Cycle = nCycleMs;
  if (arryVars)
    objDaqList.DaqList = arryVars;
  AbkHttpPut("/abk/variables/daq_list", null, objDaqList);
}




/** Installs a DAQ (data acquisition) trend at the server
@note A DAQ trend sets-up the transfer of multiple conscutive values within each DAQ cycle for one variable. It is used for high-speed signals
@param strDaqName Name for the DAQ trend
@param nCycleMs Polling cycle when reading data with long-polling
@param strVarName Names of the variable to be transferred by means of the DAQ trend
*/
function AbkSetupDaqTrend (strDaqName, nCycleMs, strVarName)
{
  var objDaqTrend = { Name: strDaqName };
  if (nCycleMs)
    objDaqTrend.Cycle = nCycleMs;
  objDaqTrend.VarName = strVarName;
  AbkHttpPut("/abk/variables/daq_trend", null, objDaqTrend);
}




/** Dynamically changes the update rate of a DAQ list
@param strDaqListName Name of the DAQ list to be changed. This DAQ list must have been already installed with AbkSetupDaqList()
@param nCycleMs New cycle in terms of milli-seconds
*/
function AbkSetDaqUpdateCycle (strDaqListName, nCycleMs)
{
  AbkHttpPut("/abk/variables/daq_list", null, { Name: strDaqListName, Cycle: nCycleMs });
}



/**Queries the real-time-clock of the server
@return The servers UTC-time
*/
function AbkGetServerTime ()
{
  var tmServer = AbkHttpGet("/abk/system_information/current_time");
  return AbkParseDate(tmServer.Time);
}



/** Sends the client state file to the server
@note A client can save its state (i.e. user settings) to the server and can recall it at re-booting.
 The client identification is done by means of class, type and serial-number
@param strClass Class of the client, e.g. "Display"
@param strType type of client, e.g. "Mytronics_superdisplay3000"
@param strSerial serial number of client.may be null, if no serial number is available due to a single - per - class system (e.g.single - display - system)
@param strWrite A client-specific formatted string of the client state
*/
function AbkPutClientState (strClass, strType, strSerial, strWrite)
{
  var strUlr = "/abk/client_states/" + strClass + "_" + strType + "_" + strSerial + ".txt";
  xhrSend.open("PUT", strUlr, false);
  xhrSend.send(strWrite);
}




/** Queries the client state file from the server
@note A client can save its state (i.e. user settings) to the server and can recall it at re-booting
 The client identification is done by means of class, type and serial-number
@param strClass Class of the client, e.g. "Display"
@param strType type of client, e.g. "Mytronics_superdisplay3000"
@param strSerial serial number of client.may be null, if no serial number is available due to a single - per - class system (e.g.single - display - system)
@return The state as it was saved with AbkPutClientState()
*/
function AbkGetClientState (strClass, strType, strSerial)
{
  var strUlr = "/abk/client_states/" + strClass + "_" + strType + "_" + strSerial + ".txt";
  xhrSend.open("GET", strUlr, false);
  AbkSetRequestHeaderNoCacheing(xhrSend);
  xhrSend.send();
  if (xhrSend.readyState == 4 && xhrSend.status == 200)
  {
    return xhrSend.responseText;
  }
  return null
}



/** Requests a form (structure and initial data) from server and formats it as an HTML form
@note Usually, a server sends an event as a request for the primary client to open a form.
@param strFormName Name of the form. It is used to a) identify the elements within the DOM and b) identify, which form needs to be requested from the server
@param strEvalOnButton Name of a function which is executed when a button is pressed.
 The function shall take two arguments:
 a) Name of the form: strFormName will be passed to this handler function
 b) Name of the button: The name of the button (which the user pressed) will be passed to this handler function.
@return An HTML-formatted form, which can be set as innerHtml of a div.
 It will contain a table with the form name and a prefix of "formtbl_" as ID. The ID may be used later to find the form in the DOM
*/
function AbkGetFormAsHtml (strFormName, strEvalOnButton)
{
  var objForm = AbkHttpGet("/abk/forms/" + strFormName);
  var strInnerHtml;
  strInnerHtml = "<h2>" + objForm.Caption + "</h2><br /><br />";
  //strInnerHtml = "<form>";
  strInnerHtml += "<table id='formtbl_" + strFormName + "' border='0' cellpadding='5' cellspacing='0'>";
  for (var nItem in objForm.Controls)
  {
    strInnerHtml += "<tr>";
    var objControl = objForm.Controls[nItem]
    var strControlName = objControl.Name;
    var strInitialValue = null;
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
        strInnerHtml += "<input type='checkbox' name='" + strControlName + "' " + strInitialValue + " " + strCheckReadOnly + " />"
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
        strInnerHtml += "<input name='" + strControlName + "' type='" + strButtonType + "' value='" + objControl.Caption + "' onclick='" + strEvalOnButton + "(\"" + strFormName + "\",\"" + strControlName + "\")' />";
        strInnerHtml += "</td>";
        break;
    }
    strInnerHtml += "</tr>";
  }
  strInnerHtml += "</table>";
  //strInnerHtml = "</form>";
  return strInnerHtml;
}



/** Submits a form to the server
@note There must be a table with an ID containing "formtbl_"+form name. When utilizing AbkGetFormAsHtml(), this scenario is provided.
@note The server may be interested in the button, the user pressed when submitting the form (e.g. was it the OK or the Cancel button?).
@param strFormName Name of the form. It is used to a) find the elements within the DOM and b) send the form name to the server
@param strFireButtonName Name of the button which initiated the submission. Emptpy string or null if no button was the initiator of the submission
*/
function AbkSendForm (strFormName, strFireButtonName)
{
  var bCloseRequired = false;
  var objReturnData = new Object();
  var tblControls = document.getElementById("formtbl_" + strFormName);
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
        value = ctrlX.checked;
        break;
      case "select-one":
        value = ctrlX.options.selectedIndex;
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

