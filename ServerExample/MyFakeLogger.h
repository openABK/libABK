//------------------------------------------------------------------------------------------------
// Author: D. Burger, Friedberg, Germany, <www.openABK.org>, <www.embu-sys.de>, <info@openABK.org>
//
// You are not allowed to remove this heading from the source code
// You are free to use this library under the terms of the
// Code Project Open Library, see <http://www.codeproject.com/info/cpol10.aspx>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//  _____  __   __  _____  _    _        ____              
// |  ___||  \ /  ||  _  \| |  | |      / ___|  _   _  ___ 
// |  __| |   ´   ||  -  /| |  | |  __  \___ \ | | | |/ __|
// | |___ | |\_/| ||  _  \| \__/ | |__|  ___) || |_| |\__ \
// |_____||_|   |_||_____/ \___ /       |____/  \__  ||___/
//                                              |___/      
// Filename:    MyFakeLogger.h
// Created:     2012-05-16 (15:20)
// Author:      D. Burger
// Description: demo logger generating faked data
//------------------------------------------------------------------------------------------------



#include "CrossPlatform.h"
#include <map>
#include <mmreg.h> // for WAVEFORMATEX

#define FAKE_VARCOUNT_MAX 2000 // number of maximum variables in the fake logger
//#define FAKE_CYCLE_MS 100 // fake animation cycle in ms
#define FAKE_VARMUTEX_TIMEOUT 1000 // timeout when giving up waiting for variable access

class CMyVarRef;

// fake variable
typedef struct tagFAKEVAR
  {
  double dValue; // actual value
  char szName[50];
  char szDisplayName[50];
  char szComment[100];
  char szUnit[20];
  char szTags[50];
  double dRangeMin;
  double dRangeMax;
  double dThresholds[5];
  double dFactor;
  double dOffset;
  double dMinOccured;
  double dMaxOccured;
  double dAvg;
  int nFractionalDigits; // desired number of fractional digits
  char *pszImageUrl; // !=NULL, the url to an image
  CMyVarRef *pVarInterface; // pointer to the variable in the abk http interface
  } FAKEVAR;

typedef struct tagFAKEMAILBOX
  {
  char strName[50];
  int nDataType; // data type of the mailbox
  union
    {
    double dValue; // numeric value of the mailbox
    char strValue[200]; // string value of the mailbox
    bool bValue; // bool value of the mailbox
    };
  } FAKEMAILBOX;



class CMyWavFileWriter
  {
  protected:
#pragma pack(push)
#pragma pack(2)
    struct WAV_RIFF
      {
      char cRiff[4];        // RIFF Header Magic header
      DWORD dwChunkSize;      // RIFF Chunk Size  
      char cWave[4];        // WAVE Header      
      };
    struct WAV_HEADER
      {
      WAV_RIFF riffWav;
      char cFmt[4];         // FMT header       
      DWORD dwSubchunk1Size;  // Size of the fmt chunk                                
      WAVEFORMATEX fmt;
      char cSubchunk2ID[4]; // "data" string   
      DWORD dwSubchunk2Size;  // Sampled data length    
      };
#pragma pack(pop)

  protected:
    FILE *m_pFile;
    WAVEFORMATEX m_fmt;
    DWORD m_dwDateaBytesWritten; // number of written data bytes

  public:
    CMyWavFileWriter ();
    ~CMyWavFileWriter ();
  public:
    bool Create (const char *pszPath, int nSamplerateHz, int nChannels, int nBitsPerSample);
    bool WriteData (const int *pSampleData, int nSamples);
    bool Close (void);
  };



class CMyLoggerInterface;

// fake logger
class CMyFakeLogger
  {
  public:
  enum
    {
    MAILBOX_ROUTES, // Streckenliste
    MAILBOX_MEDIA, // list of available transfer media, e.g. WLAN, HDD...
    MAILBOX_TRANSFER_PROGRESS, // data transfer progress
    MAILBOX_TRANSFER_STATUS, // status string of data transfer
    MAILBOX_MEASUREMENT, // 1 if measurement in progress
    MAILBOX_WRITEBACKBYCLIENT, // demonstrates editing/writing-back mailbox by the client
    MAILBOX_COUNT
    };
  enum
    {
    DATATYPE_STRING, // data type string
    DATATYPE_BOOL,
    DATATYPE_DOUBLE,
    };
  typedef std::map<int, CMyWavFileWriter> AUDIOREC_MAP;
  typedef std::pair<int, CMyWavFileWriter> AUDIOREC_PAIR;

  // data members
  protected:
    FAKEVAR m_vars[FAKE_VARCOUNT_MAX]; // variables
    FAKEMAILBOX m_mailboxes[MAILBOX_COUNT]; // mailboxes
    int m_nStorageTotal; // total storage capacity in terms of MB
    int m_nStorageAvailable; // available storage in terms of MB
    Abk::CAbkEvent m_eventSleep; // event enabling sleeping for a certain amount of time
    Abk::CAbkMutex m_mutexVars; // mutex protecting the variables
    bool m_bPaceThreadInProgress; // true as long the animation thread is in progress
    CMyLoggerInterface *m_pAbk; // pointer to the interface handling ABK client requests
    int m_nVarCount; // number of variables for animation
    int m_nAniCycle; // animation cycle in ms    
    AUDIOREC_MAP m_mapAudioRec; // currently running audio recordings
    Abk::CAbkMutex m_mutexAudioRec; // protecting m_mapAudioRec
  
  // construction/destruction
  public:
    CMyFakeLogger (int nVarCount, int nAniCycle);
    void Init (CMyLoggerInterface *pAbk); // initializes
    ~CMyFakeLogger ();
    bool Shutdown (void); // shuts down the fake logger

  // data exposing
  public:
    FAKEVAR *GetVar (int nIndex); // returns pointer to variable
    FAKEMAILBOX *GetMailbox (int nIndex); // returns pointer to mailbox
    const int GetVarCount (void) {return m_nVarCount;} // returns number of variables

  // data transfer simulation
  public:
    bool StartDataTransfer (const char *pszRoute, const char *pszStorage); // simulates a data transfer
    int GetDataTransferProgress (void); // returns progress of data transfer
    
  // misc methods and properties
  public:
    const char *GetVersion (void); // returns the version info
    int GetStorageTotal (void); // returns the total logging capacity in terms of MB
    int GetStorageAvailable (void); // returns the available logging capacity in terms of MB
    bool LockVars (void); // locks variable mutex
    void UnlockVars (void); // unlocks variable mutex
    bool StopPaceThread (void); // ends the pace thread

  // event input
  public:
    void DoVarLimit (int nVar, unsigned int nPriority, const char *pszAlertClassName, bool bNoClientSuppress); // generates a variable value limit violation
    void DoStartMeasurement (void); // simulates start of measurement
    void DoEndMeasurement (void); // simulates end of measurement

  // audio recording
  public:
    bool OnAudioRecHeader (int nId, int nSamplerateHz, int nChannels, int nBitsPerSample); // gets called when client starts an audio recording
    bool OnAudioRecData (int nId, const int *pSampleData, int nSamples); // gets called when client has audio recording data
    bool OnAudioRecFooter (int nId); // gets called when client terminates audio recording

  // implementation
  protected:
    bool Sleep (int nTimeMs); // sleeps for specified time
    static unsigned int THREAD_CALLCONV FakePace (void *pArg); // thread maintaining the faked values
    void InitMailbox (int nIndex, const char *pszName, const char *pszValue); // intis mailbox to string
    void InitMailbox (int nIndex, const char *pszName, double dValue); // intis mailbox to numeric
    void InitMailbox (int nIndex, const char *pszName, bool bValue); // intis mailbox to boolean
  };


