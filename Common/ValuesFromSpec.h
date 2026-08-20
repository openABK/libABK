// SPDX-License-Identifier: MIT

#pragma once


// values that come from the protocol specification

// the server shall respond within this period, even if no event occured
#define ABK_LONGPOLL_MAXRESPONSE_MS 2000


#define ABK_SESSION_ID_FIRST              1 // id of first session
#define ABK_SESSION_LIFETIME            120 // lifetime of a session in s

#define ABK_DAQ_CYCLE_DEFAULT           200 // default cycle time for data lists

#define ABK_VALUE_THRESHOLD_COUNT         5 // 5 thresholds resulting in 6 ranges, e.g. for colorizing

#define ABK_SERVERDISCOVER_TIMEOUT_MS   1000 // time for server to respond to a discovery request
#define ABK_SERVERDISCOVER_MAXPAYLOAD   500 // maximum payload in bytes for server discovery udp datagrams for both, requesting and answer

#define ABK_ENUM_PORT 51234 // discovery port

#define ABK_CLIENTROLE_PRIMARY "primary" // keyword for primary client if multiple clients of same class are in the system
#define ABK_CLIENTROLE_NOSPECIAL "" // no special client role

#define ABK_LIMIT_SEVERITY_LEVELS       6 // count of severity levels of limit alerts

// device classes
#define ABK_CLASSNAME_LOGGER  "Logger"
#define ABK_CLASSNAME_DISPLAY "Display"
#define ABK_CLASSNAME_KEYPAD  "Keypad"



// response url strings

// get server information
#define ABK_REQUESTURL_SERVERINFO          "/abk/system_information/server_info"
  #define ABK_RSP_SERVERINFO_PROTOVERSION   "AbkProtocolVersion"
  #define ABK_RSP_SERVERINFO_IFVERSION      "InterfaceVersion"
  #define ABK_RSP_SERVERINFO_FWVERSION      "FwVersion"
  #define ABK_RSP_SERVERINFO_HWVERSION      "HwVersion"
  #define ABK_RSP_SERVERINFO_NAME           "Name"
  #define ABK_RSP_SERVERINFO_TYPE           "Type"
  #define ABK_RSP_SERVERINFO_DESCURL        "DescriptionUrl"


// get client address
#define ABK_REQUESTURL_CLIENTADDRESS       "/abk/system_information/client_address"
  #define ABK_RSP_CLIENTADDRESS            "Address"


// current server time
#define ABK_REQUESTURL_CURRENTTIME         "/abk/system_information/current_time"
  #define ABK_RSP_CURRENTTIME_TIME          "Time"


// storage information
#define ABK_REQUESTURL_STORAGEINFO         "/abk/system_information/storage_info"
  #define ABK_RSP_STORAGEINFO_TOTAL         "Total"
  #define ABK_RSP_STORAGEINFO_FREE          "Free"


// interface statistics
#define ABK_REQUESTURL_INTERFACESTATS      "/abk/system_information/interface_statistic"


// sessions
#define ABK_REQUESTURL_SESSIONID           "/abk/system_information/session_id"
#define ABK_REQ_SESSIONID_CLASS             "Class" // client sends its class when requesting a session
#define ABK_REQ_SESSIONID_TYPE              "Type" // client sends its type when requesting a session
#define ABK_REQ_SESSIONID_SERIAL            "Serial" // client sends its serial number when requesting a session
#define ABK_REQ_SESSIONID_FWVERSION         "FwVersion" // firmware version of the requesting client
#define ABK_REQ_SESSIONID_HWVERSION         "HwVersion" // hardware version of the requesting client
#define ABK_RSP_SESSIONID_ID                "SessionId"
#define ABK_QRY_SESSIONID                   "SessionId" // the part of the querystring, e.g. location/SessionId=123


// get list of available variables/mailboxes
#define ABK_REQUESTURL_VARLIST             "/abk/variables/var_list"
#define ABK_REQUESTURL_MAILBOXLIST         "/abk/variables/mailbox_list"
  #define ABK_RSP_VARLIST                   "VarList"


// get variable/mailbox value
#define ABK_REQUESTURL_VARVALUE            "/abk/variables/var_value"
#define ABK_REQUESTURL_MAILBOXVALUE        "/abk/variables/mailbox_value"
#define ABK_REQUESTURL_MAM                 "/abk/variables/mam_values" // MinAverageMax values. only POST allowed, PUT not supported
  #define ABK_REQ_VARVALUE_GETLIST          "GetList"
  #define ABK_RSP_VARVALUE_DATA             "Data"
  #define ABK_REQ_VARVALUE_PUTLIST          "PutList"
  #define ABK_REQ_VARVALUE_NAME             "Name"
  #define ABK_REQ_VARVALUE_VALUE            "Value"


// get variable/mailbox meta information
#define ABK_REQUESTURL_VARMETA             "/abk/variables/var_meta"
#define ABK_REQUESTURL_MAILBOXMETA         "/abk/variables/mailbox_meta"
  #define ABK_REQ_VARMETA_VARLIST           "VarList"
  #define ABK_RSP_VARMETA_METADATA          "MetaData" // name of the array in the response
  #define ABK_RSP_VARMETA_NAME              "Name"
  #define ABK_RSP_VARMETA_DISPNAME          "DisplayName"
  #define ABK_RSP_VARMETA_COMMENT           "Comment"
  #define ABK_RSP_VARMETA_UNIT              "Unit"
  #define ABK_RSP_VARMETA_SYMBOL            "Symbol"
  #define ABK_RSP_VARMETA_TAGS              "Tags"
  #define ABK_RSP_VARMETA_RANGEMIN          "RangeMin"
  #define ABK_RSP_VARMETA_RANGEMAX          "RangeMax"
  #define ABK_RSP_VARMETA_FACTOR            "Factor"
  #define ABK_RSP_VARMETA_OFFSET            "Offset"
  #define ABK_RSP_VARMETA_THRESHOLDS        "Thresholds"
  #define ABK_RSP_VARMETA_FRACTDIGITS       "FractionalDigits"  // number of fractional digits for numeric representation
  #define ABK_RSP_VARMETA_OBJ_URL           "ObjUrl" // url without host name/address where clients can download an object, e.g. an image
  #define ABK_RSP_VARMETA_OBJ_MIME          "ObjMime" // type of object, e.g. image/jpeg
  #define ABK_RSP_VARMETA_TEXT              "Text"  // array with value-to-text items
  #define ABK_RSP_VARMETA_TEXTFALLBACK      "TextFallback"  // fall-back when no item of the value-to-text table matches
  #define ABK_RSP_VALTBL_VALUE              "Value" // value-to-text table: Value
  #define ABK_RSP_VALTBL_TEXT               "Text"  // value-to-text table: Text
#if defined (EXPERIMENTAL)
  #define ABK_RSP_VALTBL_VALUE_TO           "To" // value-to-text table: Value upper range (excluded)
#endif

// daq lists for variables and mailboxes
#define ABK_REQUESTURL_DAQLISTMEAS         "/abk/variables/daq_list"
#define ABK_REQUESTURL_DAQLISTMAM          "/abk/variables/mam_daq_list"
#define ABK_REQUESTURL_DAQLISTMAILBOX      "/abk/variables/mailbox_daq_list"
  #define ABK_RSP_DAQLIST_NAME              "Name"
  #define ABK_RSP_DAQLIST_CYCLE             "Cycle"
  #define ABK_RSP_DAQLIST_TEXTTRANSLATION   "TextTranslation"
  #define ABK_RSP_DAQLIST_DAQLIST           "DaqList"
  #define ABK_DEL_DAQLIST_NAME              ABK_RSP_DAQLIST_NAME

// daq for trend data
#define ABK_REQUESTURL_DAQTREND             "/abk/variables/daq_trend"
  #define ABK_RSP_DAQTREND_NAME             ABK_RSP_DAQLIST_NAME
  #define ABK_RSP_DAQTREND_CYCLE            ABK_RSP_DAQLIST_CYCLE
  #define ABK_RSP_DAQTREND_VARNAME          "VarName"

// server events
#define ABK_REQUESTURL_SERVEREVENT         "/abk/events/server_event"
  #define ABK_RSP_SERVEREVENT_DATALISTS     "DataLists"
  #define ABK_RSP_SERVEREVENT_EVENTS        "Events"
  #define ABK_RSP_SERVEREVENT_SENDER        "Sender"
  #define ABK_RSP_SERVEREVENT_TIME          "Time"
  #define ABK_RSP_SERVEREVENT_TYPE          "EventType"
  #define ABK_RSP_SERVEREVENT_ROLE          "Role"
  #define ABK_RSP_SERVEREVENT_STRPARAM      "StringParam"
  #define ABK_RSP_SERVEREVENT_PARAM1        "Param1"
  #define ABK_RSP_SERVEREVENT_PARAM2        "Param2"

  //#define ABK_RSP_DAQEVENT_LISTNAME         "Name"
  //#define ABK_RSP_DAQEVENT_LISTDATA         "Data"



// client events
#define ABK_REQUESTURL_CLIENTEVENT         "/abk/events/client_event"
  #define ABK_RSP_CLIENTEVENT_SENDER        ABK_RSP_SERVEREVENT_SENDER // same key names as server event
  #define ABK_RSP_CLIENTEVENT_PRIVATE       "Private" // server shall not reflect the event to other clients
  #define ABK_RSP_CLIENTEVENT_TIME          ABK_RSP_SERVEREVENT_TIME
  #define ABK_RSP_CLIENTEVENT_TYPE          ABK_RSP_SERVEREVENT_TYPE
  #define ABK_RSP_CLIENTEVENT_STRPARAM      ABK_RSP_SERVEREVENT_STRPARAM
  #define ABK_RSP_CLIENTEVENT_PARAM1        ABK_RSP_SERVEREVENT_PARAM1
  #define ABK_RSP_CLIENTEVENT_PARAM2        ABK_RSP_SERVEREVENT_PARAM2



// server event formattings
#define ABK_SVREVENT_MEASSTARTED            "MeasurementStarted"
#define ABK_SVREVENT_MEASSTOPPED            "MeasurementStopped"
#define ABK_SVREVENT_IDENTIFY               "Identify" // server needs a client to indentify itself (for setup purposes)
#define ABK_SVREVENT_ALERT                  "Alert"
  #define ABK_SVREVENT_ALERT_NAME             "Name"
  #define ABK_SVREVENT_ALERT_VALUE            "Value"
  #define ABK_SVREVENT_ALERT_CLASS            "Class"
  #define ABK_SVREVENT_ALERT_NOCLISUP         "_NoClientSideSuppress"
#define ABK_SVREVENT_LIMITALERT             "LimitAlert"
  #define ABK_SVREVENT_LIMITALERT_NAME        "Name"
  #define ABK_SVREVENT_LIMITALERT_VALUE       "Value"
  #define ABK_SVREVENT_LIMITALERT_CLASS       "Class"
#define ABK_SVREVENT_APPCHANGED             "AppChanged" // an app or configuration file changed. all clients must reload their apps/configs
#define ABK_SVREVENT_FORMREQUIRED           "FormRequired" // form is required to be opened by client
#define ABK_SVREVENT_FORMCLOSE              "FormClose" // form is required to be closed on clients
#define ABK_SVREVENT_VARLISTCHANGED         "VarlistChanged" // server variable list has changed, notify clients
#define ABK_SVREVENT_MESSAGE                "Message"
#define ABK_SVREVENT_MSGBOX                 "MessageBox"
  #define ABK_SVREVENT_MSGBOX_CAPTION         "Caption"
  #define ABK_SVREVENT_MSGBOX_TEXT            "Text"
  #define ABK_SVREVENT_MSGBOX_ID              "ID"
  #define ABK_SVREVENT_MSGBOX_BUTTONS         "Buttons"
    #define ABK_SVREVENT_MSGBOX_BTN_OK          "Ok"
    #define ABK_SVREVENT_MSGBOX_BTN_OKPERMA     "OkPermanent"
    #define ABK_SVREVENT_MSGBOX_BTN_YES         "Yes"
    #define ABK_SVREVENT_MSGBOX_BTN_NO          "No"
    #define ABK_SVREVENT_MSGBOX_BTN_RETRY       "Retry"
    #define ABK_SVREVENT_MSGBOX_BTN_CANCEL      "Cancel"
    #define ABK_SVREVENT_MSGBOX_BTN_IGNORE      "Ignore"
    #define ABK_SVREVENT_MSGBOX_BTN_ABORT       "Abort"

#define ABK_SVREVENT_AUDIOREC_REQ           "AudioRecRequest" // server requests that user gets a recorder display
#define ABK_SVREVENT_AUDIOREC_STOP          "AudioRecStop" // server requests that user gets a recorder display
#define ABK_REQUESTURL_AUDIOREC_HEADER      "/abk/audio_rec/header" // client sends header of audio data
  #define ABK_AUDIOREC_SAMPLERATE_HZ        "SampleRateHz" // sample rate in Hz, e.g. 22500
  #define ABK_AUDIOREC_CHANNELS             "Channels" // number of channels. 1=mono, 2=stereo
  #define ABK_AUDIOREC_BITSPERSAMPLE        "BitsPerSample" // bits of each sample of each channel. 8 or 16
  #define ABK_AUDIOREC_ID                   "ID" // general purpose ID the server wants to get reflected by the client
    #define ABK_AUDIOREC_INVALID_ID         -1  // an ID not coming from the server. This ID is used if the user initiates recording
#define ABK_REQUESTURL_AUDIOREC_DATA        "/abk/audio_rec/data" // client sends audio data
#define ABK_AUDIOREC_DATA                   "Data" // name of the data array
#define ABK_REQUESTURL_AUDIOREC_FOOTER      "/abk/audio_rec/footer" // client sends footer of audio data
#define ABK_CLIENTEVENT_AUDIOREC_REJECT     "AudioRecReject" // client notifies that user rejected audio recording

// client event formattings
#define ABK_CLIENTEVENT_BUTTON              "Button" // client notifies about button state change
#define ABK_CLIENTEVENT_WHEEL               "Wheel" // client notifies about wheel movement
#define ABK_BUTTON_LEFT                     "Left"  // names of the buttons for client button event
#define ABK_BUTTON_RIGHT                    "Right"
#define ABK_BUTTON_UP                       "Up"
#define ABK_BUTTON_DOWN                     "Down"
#define ABK_BUTTON_ENTER                    "Enter"
#define ABK_BUTTON_MENU                     "Menu"
#define ABK_BUTTON_BACK                     "Back"
#define ABK_BUTTON_F1                       "F1"
#define ABK_BUTTON_F2                       "F2"
#define ABK_BUTTON_TRIGGER                  "Trigger"

#define ABK_CLIENTEVENT_ALERT_CONFIRM       "AlertConfirm" // user confirmes an alert event
  #define ABK_ALERTCONFIRM_CLASS              "Class"       // <string> class name specification from client to server
  #define ABK_ALERTCONFIRM_SEVERITY           "Severity"    // <int> severity field when client notifies to server
  #define ABK_ALERTCONFIRM_COUNT              "Count"       // <int> number of merged alerts the user confirms (e.g. 3 if user clicks a button to confirm all 3 alerts merged into one message box)
  #define ABK_ALERTCONFIRM_SUPPRESSED         "Suppressed"  // <bool> the confirmation was generated automatically since alert is not visible to user (suppressed to user)
  #define ABK_ALERTCONFIRM_TIMEOUT            "Timeout"  // <bool> the confirmation was generated automatically since alert is not visible to user (suppressed to user)
  #define ABK_ALERTCONFIRM_PERMASUPPRBYUSER   "PermanentSuppressedByUser" // <bool>. If true, user selected permanent suppression of further messages of same kind

#define ABK_CLIENTEVENT_MSGBOX_CONFIRM      "MessageBoxConfirm" // user confirmes message box

// client firmware information
#define ABK_REQUESTURL_FIRMWARE             "/abk/system_information/client_firmware"
#define ABK_REQ_FIRMWARE_CLASS              "Class" // client sends its class when requesting firmware information
#define ABK_REQ_FIRMWARE_TYPE               "Type" // client sends its type when requesting firmware information
#define ABK_RSP_FIRMWARE_IMAGELIST          "Images" // name of the array containing firmware information images
#define ABK_RSP_FIRMWARE_VERSION            "Version" // server codes the version of an available client image
#define ABK_RSP_FIRMWARE_URL                "Url" // server provides url where client can load its firmware image
#define ABK_RSP_FIRMWARE_MD5                "MD5" // server provides hash of the client firmware image

// client configuration information
#define ABK_REQUESTURL_CLIENTCONFIG_INFO    "/abk/client_config/info"       // clients get their apps from this directory
#define ABK_REQ_CLIENTCONFIG_CLASS          "Class" // client sends its class when requesting config information
#define ABK_REQ_CLIENTCONFIG_TYPE           "Type" // client sends its type when requesting config information
#define ABK_REQ_CLIENTCONFIG_SERIAL         "Serial" // client sends its serial number when requesting config information
#define ABK_RSP_CLIENTCONFIG_URL            "Url" // server provides url where client can load its config file
#define ABK_RSP_CLIENTCONFIG_MD5            "MD5" // server provides hash of the client info file

// file services
#define ABK_SERVICE_FILES                   "/abk/files"                    // general purpose file directory
#define ABK_SERVICE_CLIENTSTATES            "/abk/client_states"            // client can store the user settings into this direcotry
#define ABK_SERVICE_CLIENTCONFIG            "/abk/client_config/files"      // clients get their apps from this directory (recommendation by this implementation)
#define ABK_SERVICE_CLIENTFIRMWARE          "/abk/firmware"                 // clients get their firmware from this direcotry (recommendation by this implementation)


// forms
#define ABK_SERVICE_FORMS                   "/abk/forms" // client can get forms and put the result
#define ABK_RSP_FORMS_CAPTION               "Caption"
#define ABK_RSP_FORMS_CONTROLS              "Controls"
#define ABK_RSP_FORMS_CONTROLTYPE           "Type"
#define ABK_RSP_FORMS_CONTROLTYPE_INPUT     "Input"
#define ABK_RSP_FORMS_CONTROLTYPE_CHECKBOX  "Checkbox"
#define ABK_RSP_FORMS_CONTROLTYPE_COMBO      "List"
#define ABK_RSP_FORMS_CONTROLTYPE_BUTTON    "Button"
#define ABK_RSP_FORMS_CONTROL_NAME          "Name"
#define ABK_RSP_FORMS_CONTROL_CAPTION       "Caption"
#define ABK_RSP_FORMS_CONTROL_INITIALVALUE  "InitialValue"
#define ABK_RSP_FORMS_CONTROL_MAXLEN        "MaxLen"
#define ABK_RSP_FORMS_CONTROL_PASSWORD      "Password"
#define ABK_RSP_FORMS_CONTROL_READONLY      "ReadOnly"
#define ABK_RSP_FORMS_CONTROL_NUMERIC       "Numeric"
#define ABK_RSP_FORMS_CONTROL_OPTIONS       "Options"
#define ABK_RSP_FORMS_CONTROL_SUBMIT        "Submit"
#define ABK_RSP_FORMS_CONTROL_CANCEL        "Cancel"
#define ABK_RSP_FORMS_CONTROL_UPDATEABLE    "Updateable"   // form element is dynamically updateable if this attribute is set to true
#define ABK_RSP_FORMS_PERSISTENCE           "Persistence" // specifies time after which the form shall close automatically


// server discovery
#define ABK_DISCOVERY_REQ_KEYNAME           "Key"
#define ABK_DISCOVERY_REQ_KEYVALUE          "AbkSearchForServer"
#define ABK_DISCOVERY_REQ_CLASS             "Class" // client sends its class when requesting an answer
#define ABK_DISCOVERY_REQ_TYPE              "Type" // client sends its type when requesting an answer
#define ABK_DISCOVERY_REQ_SERIAL            "Serial" // client sends its serial number when requesting an answer

#define ABK_DISCOVERY_RSP_KEYNAME           "Key"
#define ABK_DISCOVERY_RSP_KEYVALUE          "AbkServer"
#define ABK_DISCOVERY_RSP_ADDRESS           "Address"   // address via it a client can connect to the server
#define ABK_DISCOVERY_RSP_PORT              "Port"      // port an http request can be sent
#define ABK_DISCOVERY_RSP_CLASS             "Class"     // server sends its class when answering the request
#define ABK_DISCOVERY_RSP_TYPE              "Type"      // server sends its type when answering the request
#define ABK_DISCOVERY_RSP_SERIAL            "Serial"    // server sends its serial number when answering the request
#define ABK_DISCOVERY_RSP_NAME              "Name"      // unique name of the server within the system
#define ABK_DISCOVERY_RSP_PREFERRED         "Preferred" // true if connection in a configuration
#define ABK_DISCOVERY_RSP_DESCURL           "DescriptionUrl" // description of the server, optional

