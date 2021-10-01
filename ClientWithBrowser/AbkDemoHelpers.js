

// globals
var g_nButtonStates = new Array(); // current state of each button, used to track and inhibit redundant events


/** Writes an empty table to the current document
@param nHeight Number of rows to be created
@param strId ID tag for the new table
@param nColWidths Array of column widths. Each entity can contain either a numeric value or a string with units (e.g. 60%).
@param strCaptions Array of strings with the captions for the first row
*/
function WriteEmptyTable(nHeight, strId, nColWidths, strCaptions)
{
  var strHtml = WriteEmptyTableToString(nWidth, nHeight, strId, nColWidths, strCaptions);
  document.writeln(strHtml);
}



/** Formats an empty table to a string
@param nHeight Number of rows to be created
@param strId ID tag for the new table
@param nColWidths Array of column widths. Each entity can contain either a numeric value or a string with units (e.g. 60%).
@param strCaptions Array of strings with the captions for the first row
@return The html-formatted string of the table
*/
function WriteEmptyTableToString(nHeight, strId, nColWidths, strCaptions)
{
  var strOut = "";
  strOut += "<table id=\"";
  strOut += strId;
  strOut += "\"border=\"1\" cellpadding=\"2\" cellspacing=\"2\">";
  var nWidth = strCaptions.length;

  strOut += "<colgroup>";
  for (nCol = 0; nCol < nWidth; nCol++)
  {
    strOut += "<col width=\"" + nColWidths[nCol] + "\">";
  }
  strOut += "</colgroup>";

  // strOut+="<tr><td>Name</td><td>Value</td><td>Unit</td><td>Comment</td></tr>");
  var strBgCol = ["FFFFFF", "ABCDEF"];
  for (nY = 0; nY < nHeight; nY++)
  {
    strOut += "<tr>";
    for (nX = 0; nX < nWidth; nX++)
    {
      strOut += "<td id=\"Cell";
      //strOut+="<td";
      strOut += nX + nY * nWidth;
      strOut += "\"style=\"background-color:#" + strBgCol[nY % 2] + "\">&nbsp;";
      if (nY == 0)
        strOut += strCaptions[nX];
      strOut += "</td>";
    }
    strOut += "</tr>";
  }
  strOut += "</table>";
  return strOut;
}



/** Formats a tile which shows a variable value in a large manner
@param nIdSuffix The suffix string which will be appended to the ID tag of several items. These items are:
 cbLargeSelect Combo box <select> the user can select the variable
 tblLarge A table <table> where the elements are placed into 
 largeName The table <td> where the name of the variable will be shown
 largeValue The table <td> where the value will be shown
 largeUnit The table <td> where the unit will be shown
@return The html-formatted string for the variable display
*/
function WriteLargeDisplayToString(nIdSuffix)
{
  var strHtml = "";
  strHtml += "<select size='1' style='width: 200px' id='cbLargeSelect" + nIdSuffix + "' onchange='SyncForLargeSelect(this.options[this.options.selectedIndex].value)'>";
  strHtml += "</select><br/><br/>";
  strHtml += "<table border='1' cellpadding='5' cellspacing='0' id='tblLarge" + nIdSuffix + "'>";
  strHtml += "  <colgroup>";
  strHtml += "    <col width='200' />";
  strHtml += "    <col width='200' />";
  strHtml += "    <col width='200' />";
  strHtml += "  </colgroup>";
  strHtml += "  <tr>";
  strHtml += "    <td id='largeName" + nIdSuffix + "' width='20px' valign='bottom'>";
  strHtml += "    </td>";
  strHtml += "    <td height='75' id='largeValue" + nIdSuffix + "' width='500px' valign='bottom' align='right' style='font-size: 50px'>";
  strHtml += "    [value]</td>";
  strHtml += "    <td id='largeUnit" + nIdSuffix + "' width='20' valign='bottom'>";
  strHtml += "    </td>";
  strHtml += "  </tr>";
  strHtml += "</table>";
  return strHtml;
}




/** Fills a combo box with variable names
@param strComboId ID of the combo box to be filled
@param objVarList An array of strings holding the variable names
*/
function FillComboWithVars(strComboId, objVarList)
{
  var cbTarget = document.getElementById(strComboId);
  for (var nVar in objVarList)
  {
    var strVarName = objVarList[nVar];
    var elOptNew = document.createElement('option');
    elOptNew.text = strVarName;
    elOptNew.value = strVarName;
    cbTarget.add(elOptNew, null);
  }
}



function HandleButtonMouseDown(strId)
{
  if (!g_nButtonStates[strId])
    g_nButtonStates[strId] = 0;
  if (g_nButtonStates[strId] == 0)
  {
    g_nButtonStates[strId] = 1;
    strId = strId.slice(3); // remove the 'btn' prefix
    AbkSendEvent("Button", strId, 1, 0,true);
  }
}

function HandleButtonMouseUp(strId)
{
  if (!g_nButtonStates[strId])
    g_nButtonStates[strId] = 0;
  if (g_nButtonStates[strId] != 0)
  {
    g_nButtonStates[strId] = 0;
    strId = strId.slice(3); // remove the 'btn' prefix
    AbkSendEvent("Button", strId, 0, 0, true);
  }
}

function HandleButtonMouseOut(strId)
{
  if (!g_nButtonStates[strId])
    g_nButtonStates[strId] = 0;
  if (g_nButtonStates[strId] != 0)
  {
    g_nButtonStates[strId] = 0;
    strId = strId.slice(3); // remove the 'btn' prefix
    AbkSendEvent("Button", strId, 0, 0, true);
  }
}
