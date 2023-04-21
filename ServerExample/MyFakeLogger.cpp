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
// Filename:    MyFakeLogger.cpp
// Created:     2012-05-16 (15:20)
// Author:      D. Burger
// Description: demo logger generating faked data
//------------------------------------------------------------------------------------------------




#include "stdafx.h"
#include <assert.h>
#include "float.h"
#include <limits>
#include "MyFakeLogger.h"
#include "MyLoggerInterface.h"

#pragma warning(disable : 4996)


//#define TESTBENCH_ABKBUTTONS


using namespace std;





//--------------------------------------------------------------------------
// CMyWavFileWriter()      Constructor of CMyWavFileWriter
// ------------------
// Input: -
// Return: 

CMyWavFileWriter::CMyWavFileWriter ()
  {
  m_pFile=NULL;
  }



//--------------------------------------------------------------------------
// ~CMyWavFileWriter()     Destructor of CMyWavFileWriter
// -------------------
// Input: -
// Return: 

CMyWavFileWriter::~CMyWavFileWriter ()
  {
  if(m_pFile)
    Close();
  }



//--------------------------------------------------------------------------
// Create()                creates a file
// --------
// Input: pszPath = 
//        nSamplerateHz = 
//        nChannels = 
//        nBitsPerSample = 
// Return: 

bool CMyWavFileWriter::Create (const char *pszPath, int nSamplerateHz, int nChannels, int nBitsPerSample)
  {
  bool bSuccess=FALSE;
  ASSERT(nBitsPerSample==8 || nBitsPerSample==16); // invalid bits per sample
  ASSERT(nChannels==1 || nChannels==2); // olny mono or stereo
  if((nBitsPerSample==8 || nBitsPerSample==16) && (nChannels==1 || nChannels==2))
    {
    m_fmt.cbSize=sizeof(m_fmt);
    m_fmt.wFormatTag=WAVE_FORMAT_PCM;
    m_fmt.nChannels=nChannels;
    m_fmt.nSamplesPerSec=nSamplerateHz;
    m_fmt.wBitsPerSample=nBitsPerSample;
    m_fmt.nBlockAlign=m_fmt.nChannels*m_fmt.wBitsPerSample/8;
    m_fmt.nAvgBytesPerSec=m_fmt.nSamplesPerSec*m_fmt.nBlockAlign;

    m_pFile=fopen(pszPath,"wb");
    if(m_pFile)
      {
      m_dwDateaBytesWritten=0;
      WAV_HEADER hdr;
      ZeroMemory(&hdr,sizeof(hdr));
      DWORD dwWritten=0;
      dwWritten=fwrite(&hdr,1,sizeof(hdr),m_pFile); // write a nonsense header, we put the data on finalizing
      bSuccess=(dwWritten==sizeof(hdr));
      }
    }
  return bSuccess;
  }



//--------------------------------------------------------------------------
// WriteData()             writes sample data
// -----------
// Input: pSampleData = data. Number of entities must be ChannelCount * nSamples
//                      if multiple channels (n): first n data are the first samples of n channels and so on
//        nSamples = number of samples of this chunk (of all channels togehter)
// Return: true on success, FALSE on error

bool CMyWavFileWriter::WriteData (const int *pSampleData, int nSamples)
  {
  bool bSuccess=FALSE;
  ASSERT(m_pFile);
  if(m_pFile)
    {
    if(m_fmt.wBitsPerSample==8)
      {
      int nBufferSize=nSamples*sizeof(BYTE);
      BYTE *pBuffer=(BYTE *)malloc(nBufferSize);
      BYTE *pWrite=pBuffer;
      ZeroMemory(pWrite,nBufferSize);
      for(int nData=0;nData<nBufferSize;nData+=sizeof(BYTE))
        {
        *pWrite=*pSampleData;
        ++pWrite;
        ++pSampleData;
        }
      size_t nWritten=fwrite(pBuffer,1,nBufferSize,m_pFile);
      m_dwDateaBytesWritten+=nWritten;
      bSuccess=nWritten==nBufferSize;
      }
    else if(m_fmt.wBitsPerSample==16)
      {
      int nBufferSize=nSamples*sizeof(WORD);
      WORD *pBuffer=(WORD *)malloc(nBufferSize);
      WORD *pWrite=pBuffer;
      ZeroMemory(pWrite,nBufferSize);
      for(int nData=0;nData<nBufferSize;nData+=sizeof(WORD))
        {
        *pWrite=*pSampleData;
        ++pWrite;
        ++pSampleData;
        }
      size_t nWritten=fwrite(pBuffer,1,nBufferSize,m_pFile);
      m_dwDateaBytesWritten+=nWritten;
      bSuccess=nWritten==nBufferSize;
      }
    else
      {
      ASSERT(FALSE); // invalid bits per sample
      }
    }
  return bSuccess;
  }



//--------------------------------------------------------------------------
// Close()                 closes the file
// -------
// Input: -
// Return: true on success, FALSE on error

bool CMyWavFileWriter::Close (void)
  {
  bool bSuccess=FALSE;
  ASSERT(m_pFile);
  if(m_pFile)
    {
    if(!fseek(m_pFile,0,SEEK_SET))
      {
      WAV_HEADER hdr;
      memcpy(&hdr.fmt,&m_fmt,sizeof(m_fmt));
      memcpy(hdr.riffWav.cRiff,"RIFF",sizeof(hdr.riffWav.cRiff));
      hdr.riffWav.dwChunkSize=m_dwDateaBytesWritten+sizeof(hdr)-sizeof(hdr.riffWav.dwChunkSize)-sizeof(hdr.riffWav.cRiff);
      memcpy(hdr.riffWav.cWave,"WAVE",sizeof(hdr.riffWav.cWave));
      memcpy(hdr.cFmt,"fmt ",sizeof(hdr.cFmt));
      hdr.dwSubchunk1Size=sizeof(hdr.fmt);
      memcpy(hdr.cSubchunk2ID,"data",sizeof(hdr.cSubchunk2ID));
      hdr.dwSubchunk2Size=m_dwDateaBytesWritten;
      size_t nWritten=fwrite(&hdr,1,sizeof(hdr),m_pFile);
      bSuccess=nWritten==sizeof(hdr);
      }
    fclose(m_pFile);
    m_pFile=NULL;
    m_dwDateaBytesWritten=0;
    }
  return bSuccess;
  }
















//--------------------------------------------------------------------------
// CMyFakeLogger()         Constructor of CMyFakeLogger
// ---------------
// Input: -
// Return: -

CMyFakeLogger::CMyFakeLogger (int nVarCount, int nAniCycle)
  : m_pAbk(NULL)
  {
  m_nStorageTotal=1024; // fake 1GB of memory available
  m_nStorageAvailable=m_nStorageTotal;
  m_nVarCount=nVarCount;
  m_nAniCycle=nAniCycle;

  // init the fake variables
  memset(m_vars,0,sizeof(m_vars));
  int nVar;
  for(nVar=0;nVar<FAKE_VARCOUNT_MAX;nVar++)
    {
    m_vars[nVar].dRangeMin=0.;
    m_vars[nVar].dRangeMax=100.;

#if defined (TESTBENCH_ABKBUTTONS)
    m_vars[nVar].dValue=nVar;
#else
    m_vars[nVar].dValue=0.;
#endif
    m_vars[nVar].dFactor=1.;
    m_vars[nVar].dOffset=0.;
    m_vars[nVar].dThresholds[0]=-numeric_limits<double>::infinity();
    m_vars[nVar].dThresholds[1]=-numeric_limits<double>::infinity();
    m_vars[nVar].dThresholds[2]=0.;
    m_vars[nVar].dThresholds[3]=numeric_limits<double>::infinity();
    m_vars[nVar].dThresholds[4]=numeric_limits<double>::infinity();
    m_vars[nVar].dMinOccured=0.;
    m_vars[nVar].dMaxOccured=0.;
    m_vars[nVar].dAvg=0.;
    m_vars[nVar].dAvg=0.;
    m_vars[nVar].nFractionalDigits=0;
    m_vars[nVar].pszImageUrl=NULL;
#if defined (TESTBENCH_ABKBUTTONS)
    sprintf(m_vars[nVar].szName,"openABK_Button_%d",nVar);
    sprintf(m_vars[nVar].szDisplayName,"openABK_Button_%d",nVar);
#else
    sprintf(m_vars[nVar].szName,"Vari%d",nVar);
    sprintf(m_vars[nVar].szDisplayName,"Variable%d",nVar);
#endif
    sprintf(m_vars[nVar].szUnit,"Unit%d",nVar);
    sprintf(m_vars[nVar].szComment,"Comment%d",nVar);
    }

#if defined (TESTBENCH_ABKBUTTONS)
#else
  // Var #0 specialties
  sprintf(m_vars[0].szName,"Tirepressure"); // "Tire.p<r>essure"
  sprintf(m_vars[0].szDisplayName,"Tire-pressure");
  sprintf(m_vars[0].szUnit,"bar");
  sprintf(m_vars[0].szComment,"{en}manually generated variable{de}manuell erstellte Variable");
  m_vars[0].dFactor=0.01;
  m_vars[0].dOffset=1.8;
  m_vars[0].dThresholds[0]=15;
  m_vars[0].dThresholds[1]=20;
  m_vars[0].dThresholds[2]=25;
  m_vars[0].dThresholds[3]=28;
  m_vars[0].dThresholds[4]=40;
  m_vars[0].nFractionalDigits=3;
  CVarRef::CMeta::CValueTable tblValToString;
  m_vars[0].m_tblValToText.Add (22, 23, "Text at 22");
  m_vars[0].m_tblValToText.Add (23, 24, "Text at 23");
  m_vars[0].m_tblValToText.SetFallback ("fall-back 0");

  // Var #3 specialties
  // m_vars[3].nFractionalDigits = 4;

  // Var #4, #5 specialties
  m_vars[4].pszImageUrl="abk/jpegs/Clip_480_5sec_6mbps_h264.jpg";
  strcpy(m_vars[4].szTags,"Video JPG");
  m_vars[5].pszImageUrl="abk/jpegs/Video2.jpg";
  strcpy(m_vars[5].szTags,"Video JPG");
#endif

  // init the fake mailboxes
  memset(&m_mailboxes,0,sizeof(m_mailboxes));
  InitMailbox(MAILBOX_ROUTES,"Streckenliste",234567895.  /*"Wolnzach;Regensburg;Ulm"*/);
  InitMailbox(MAILBOX_MEDIA,"StorageMedia","WLAN,WLAN\nHDD,Local HDD");
  InitMailbox(MAILBOX_TRANSFER_PROGRESS,"TransferProgress",-1.); // currently no transfer in progress
  InitMailbox(MAILBOX_TRANSFER_STATUS,"TransferStatus",""); // currently no transfer in progress
  InitMailbox(MAILBOX_MEASUREMENT,"MeasurementInProgress",false); // currently no transfer in progress
  InitMailbox(MAILBOX_WRITEBACKBYCLIENT,"WriteBackData","String Value"); // editable/write-back demo
  }



//--------------------------------------------------------------------------
// Init()                  initializes
// ------
// Input: pAbk = pointer to the interface handling ABK client requests
// Return: -

void CMyFakeLogger::Init (CMyLoggerInterface *pAbk)
  {
  m_pAbk=pAbk;
  m_bPaceThreadInProgress=true;
  AbkStartThread(FakePace,this); // start the animation thread
  }



//--------------------------------------------------------------------------
// ~CMyFakeLogger()        Destructor of CMyFakeLogger
// ----------------
// Input: -
// Return: 

CMyFakeLogger::~CMyFakeLogger ()
  {
  assert(!m_bPaceThreadInProgress); // you have to shutdown the logger with Shutdown before destuction
  }


//--------------------------------------------------------------------------
// Shutdown()              shuts down the fake logger
// ----------
// Input: -
// Return: true on success false on error

bool CMyFakeLogger::Shutdown (void)
  {
  return StopPaceThread();
  }



//--------------------------------------------------------------------------
// StopPaceThread()        stops the pace heartbeat thread of the logger
// ----------------
// Input: -
// Return: true on success false on error

bool CMyFakeLogger::StopPaceThread (void)
  {
  m_eventSleep.Set(); // tell the animation thread to terminate
  while(m_bPaceThreadInProgress) // wait for the thread to terminate
    AbkSleepMs(10);
  return true;
  }


//--------------------------------------------------------------------------
// InitMailbox()           intis mailbox to string
// -------------
// Input: nIndex = index of mailbox
//        strName = name for the mailbox
//        strValue = string value for the mailbox
// Return: -

void CMyFakeLogger::InitMailbox (int nIndex, const char *pszName, const char *pszValue)
  {
  strncpy(m_mailboxes[nIndex].strName,pszName,sizeof(m_mailboxes[0].strName));
  strncpy(m_mailboxes[nIndex].strValue,pszValue,sizeof(m_mailboxes[0].strValue));
  m_mailboxes[nIndex].nDataType=CMyFakeLogger::DATATYPE_STRING;
  }

//--------------------------------------------------------------------------
// InitMailbox()           intis mailbox to numeric
// -------------
// Input: nIndex = index of mailbox
//        strName = name for the mailbox
//        dValue = value for the mailbox
// Return: -

void CMyFakeLogger::InitMailbox (int nIndex, const char *pszName, double dValue)
  {
  strncpy(m_mailboxes[nIndex].strName,pszName,sizeof(m_mailboxes[0].strName));
  m_mailboxes[nIndex].dValue=dValue;
  m_mailboxes[nIndex].nDataType=CMyFakeLogger::DATATYPE_DOUBLE;
  }

//--------------------------------------------------------------------------
// InitMailbox()           intis mailbox to boolean
// -------------
// Input: nIndex = index of mailbox
//        strName = name for the mailbox 
//        bValue = value for the mailbox
// Return: 

void CMyFakeLogger::InitMailbox (int nIndex, const char *pszName, bool bValue)
  {
  strncpy(m_mailboxes[nIndex].strName,pszName,sizeof(m_mailboxes[0].strName));
  m_mailboxes[nIndex].bValue=bValue;
  m_mailboxes[nIndex].nDataType=CMyFakeLogger::DATATYPE_BOOL;
  }


//--------------------------------------------------------------------------
// Sleep()                 sleeps for specified time
// -------
// Input: nTimeMs = time in ms
// Return: true if sleep succeeded, false if logger shall terminate

bool CMyFakeLogger::Sleep (int nTimeMs)
  {
  return !m_eventSleep.Wait(nTimeMs);  
  }




//--------------------------------------------------------------------------
// FakePace()              thread maintaining the faked values
// ----------
// Input: pArg = pointer to fake logger
// Return: always 0

/*static*/ unsigned int THREAD_CALLCONV CMyFakeLogger::FakePace (void *pArg)
  {
  CMyFakeLogger *pThis=reinterpret_cast<CMyFakeLogger *>(pArg);
  int nPrescale1=0; // prescale 1 sec
  int nPrescale10=0; // prescale 10 sec

  for(;;)
    {
    if(!pThis->Sleep(pThis->m_nAniCycle))
      break; // shall terminate

    // maintain prescalers
    if(!nPrescale10)
      nPrescale10=10*1000/pThis->m_nAniCycle;
    nPrescale10--;
    if(!nPrescale1)
      nPrescale1=1*1000/pThis->m_nAniCycle;
    nPrescale1--;
    
    // animate the values
    CAbkSingleLock guard(&pThis->m_mutexVars,true,FAKE_VARMUTEX_TIMEOUT); // gain access to the variables etc.
#if defined (TESTBENCH_ABKBUTTONS)
    int nVar;
    for(nVar=0;nVar<pThis->m_nVarCount;nVar++)
      {
      FAKEVAR *pLoggerVar=&pThis->m_vars[nVar];
      pLoggerVar->dValue=(double)(((int)(pLoggerVar->dValue)+1)%16);
      //CMyVarRef *pVarInterface=pLoggerVar->pVarInterface;
      //if(pVarInterface)
      //  pVarInterface->FeedTrend();
      }
#else
    int nVar;
    for(nVar=0;nVar<pThis->m_nVarCount;nVar++)
      {
      FAKEVAR *pLoggerVar=&pThis->m_vars[nVar];
      pLoggerVar->dValue++;
      CMyVarRef *pVarInterface=pLoggerVar->pVarInterface;
      if(pVarInterface)
        pVarInterface->FeedTrend();      
      }
    
    pThis->m_vars[0].dValue=(double)((int)(pThis->m_vars[0].dValue)%100); // ####

    pThis->m_vars[2].dValue=pThis->m_vars[1].dValue;
    if(((int)(pThis->m_vars[2].dValue)%100)<20)
      pThis->m_vars[2].dValue=std::numeric_limits<double>::quiet_NaN();

  //  if(GetKeyState(VK_SHIFT)&0x8000)
      pThis->m_vars[3].dValue=(double)0x113F17; //####

    // maintain min/average/max
    for(nVar=0;nVar<pThis->m_nVarCount;nVar++)
      {
      FAKEVAR *pLoggerVar=&pThis->m_vars[nVar];
      if(pLoggerVar->dValue>pLoggerVar->dMaxOccured)
        pLoggerVar->dMaxOccured=pLoggerVar->dValue;
      if(pLoggerVar->dValue<pLoggerVar->dMinOccured)
        pLoggerVar->dMinOccured=pLoggerVar->dValue;
      pLoggerVar->dAvg=(pLoggerVar->dAvg*15.+pLoggerVar->dValue)/16.; // averaging
      }
#endif

    // animate the storage capacity
    if((!nPrescale10)&&(pThis->m_nStorageAvailable>0))
      pThis->m_nStorageAvailable--;

    // animate data transfer
    FAKEMAILBOX *pMbTransferProgress=&pThis->m_mailboxes[MAILBOX_TRANSFER_PROGRESS];
    FAKEMAILBOX *pMbTransferStatus=&pThis->m_mailboxes[MAILBOX_TRANSFER_STATUS];
    if(pMbTransferProgress->dValue>=0)
      {
      pMbTransferProgress->dValue++;
      sprintf(pMbTransferStatus->strValue,"MyFakeLogger: Transfer in progress: %d %%",(int)pMbTransferProgress->dValue);
      if(pMbTransferProgress->dValue>=101)
        {
        pMbTransferProgress->dValue=-1;
        pMbTransferStatus->strValue[0]='\0';
        }
      }


    }
  pThis->m_bPaceThreadInProgress=false; // tell the calling/terminating thread that we terminated
  return 0;
  }




//--------------------------------------------------------------------------
// GetVar()                returns pointer to variable
// --------
// Input: nIndex = index of variable
// Return: pointer to variable

FAKEVAR *CMyFakeLogger::GetVar (int nIndex)
  {
  assert((nIndex>=0)&&(nIndex<m_nVarCount));
  return &m_vars[nIndex]; // return the variable
  }


//--------------------------------------------------------------------------
// GetMailbox()            returns pointer to mailbox
// ------------
// Input: nIndex = index of mailbox
// Return: pointer to mailbox

FAKEMAILBOX *CMyFakeLogger::GetMailbox (int nIndex)
  {
  assert((nIndex>=0)&&(nIndex<MAILBOX_COUNT));
  return &m_mailboxes[nIndex]; // return the mailbox
  }




//--------------------------------------------------------------------------
// GetVersion()            returns the version info
// ------------
// Input: -
// Return: version info, ment as firmware revision

const char *CMyFakeLogger::GetVersion (void)
  {
  return "1.0";
  }




//--------------------------------------------------------------------------
// GetStorageTotal()       returns the total logging capacity in terms of MB
// -----------------
// Input: -
// Return: total logging capacity in terms of MB

int CMyFakeLogger::GetStorageTotal (void)
  {
  CAbkSingleLock guard(&m_mutexVars,true,FAKE_VARMUTEX_TIMEOUT); // gain access to the variables and other members
  return m_nStorageTotal;
  }




//--------------------------------------------------------------------------
// GetStorageAvailable()   returns the available logging capacity in terms of MB
// ---------------------
// Input: -
// Return: available logging capacity in terms of MB

int CMyFakeLogger::GetStorageAvailable (void)
  {
  CAbkSingleLock guard(&m_mutexVars,true,FAKE_VARMUTEX_TIMEOUT); // gain access to the variables and other members
  return m_nStorageAvailable;
  }





//--------------------------------------------------------------------------
// LockVars()              locks variable mutex
// ----------
// Input: -
// Return: true on success, false on error or timeout

bool CMyFakeLogger::LockVars (void)
  {
  return m_mutexVars.Lock(FAKE_VARMUTEX_TIMEOUT);
  }


//--------------------------------------------------------------------------
// UnlockVars()            unlocks variable mutex
// ------------
// Input: -
// Return: 

void CMyFakeLogger::UnlockVars (void)
  {
  m_mutexVars.Unlock();
  }






//--------------------------------------------------------------------------
// StartDataTransfer()     simulates a data transfer
// -------------------
// Input: strRoute = route the driver entered
//        strStorage = storage media the user entered
// Return: true on success, false on error

bool CMyFakeLogger::StartDataTransfer (const char *pszRoute, const char *pszStorage)
  {
  printf("Data transfer started. Route: %s, Storage: %s\n",pszRoute,pszStorage);
  LockVars();
  m_mailboxes[MAILBOX_TRANSFER_PROGRESS].dValue=0;
  UnlockVars();
  return true;
  }


//--------------------------------------------------------------------------
// GetDataTransferProgress() returns progress of data transfer
// -------------------------
// Input: -
// Return: progress in terms of %. <0 if currently no transfer in progress

int CMyFakeLogger::GetDataTransferProgress (void)
  {
  int nProgress;
  LockVars();
  nProgress=(int)m_mailboxes[MAILBOX_TRANSFER_PROGRESS].dValue;  
  UnlockVars();
  return nProgress;
  }




//--------------------------------------------------------------------------
// DoVarLimit()            generates a variable value limit violation
// ------------
// Input: nVar = variable index for which the limit shall be generated
//        pszAlertClassName = class name for the alert
//        NoClientSuppress = true if client shall not suppress this and further
//                           similar alerts
// Return: -

void CMyFakeLogger::DoVarLimit (int nVar, unsigned int nPriority, const char *pszAlertClassName, bool bNoClientSuppress)
  {
  if(nVar>=m_nVarCount)
    return;
  CJsonFormatter jfAdditionalFields;
  jfAdditionalFields.WriteValue("Title","Fake-Logger Alert");
  jfAdditionalFields.Close();
  m_pAbk->Alert(m_vars[nVar].szName,pszAlertClassName,nPriority,0,bNoClientSuppress,&jfAdditionalFields);
  }




//--------------------------------------------------------------------------
// DoStartMeasurement()    simulates start of measurement
// --------------------
// Input: -
// Return: 

void CMyFakeLogger::DoStartMeasurement (void)
  {
  FAKEMAILBOX *pMailbox=GetMailbox(MAILBOX_MEASUREMENT);
  ASSERT(pMailbox);
  LockVars();
  pMailbox->bValue=true;
  UnlockVars();
  m_pAbk->FireEventToAllClientsMeasurementStarted();
  }


//--------------------------------------------------------------------------
// DoEndMeasurement()      simulates end of measurement
// ------------------
// Input: -
// Return: 

void CMyFakeLogger::DoEndMeasurement (void)
  {
  FAKEMAILBOX *pMailbox=GetMailbox(MAILBOX_MEASUREMENT);
  ASSERT(pMailbox);
  LockVars();
  pMailbox->bValue=false;
  UnlockVars();
  m_pAbk->FireEventToAllClientsMeasurementStopped();
  }




//--------------------------------------------------------------------------
// OnAudioRecHeader()      gets called when client starts an audio recording
// ------------------
// Input: nId = general purpose ID the server wanted to be reflected by the client
//        nSamplerateHz = sample rate in Hz
//        nChannels = number of channels (1=mono, 2=stereo)
//        nBitsPerSample = number of bits per sample (8 or 16)
// Return: true if e.g. file could be created successfully. false otherwise

bool CMyFakeLogger::OnAudioRecHeader (int nId, int nSamplerateHz, int nChannels, int nBitsPerSample)
  {
  bool bSuccess=false;
  Abk::CAbkSingleLock guardAudioRec(&m_mutexAudioRec,true);
  AUDIOREC_MAP::iterator itAudioRec=m_mapAudioRec.find(nId);
  if(itAudioRec!=m_mapAudioRec.end()) // recording with this ID already running, close and delete it
    {
    itAudioRec->second.Close();
    m_mapAudioRec.erase(itAudioRec);
    }
  std::pair<AUDIOREC_MAP::iterator,bool> pairNew=m_mapAudioRec.insert(AUDIOREC_PAIR(nId,CMyWavFileWriter()));
  assert(pairNew.second==true); // why is there still an entity in the map??
  if(pairNew.second==true)
    {
    CMyWavFileWriter &rWavWriter=pairNew.first->second;
    char cFilePath[MAX_PATH];
    sprintf(cFilePath,"%sWavRecoring%d.wav",LOCDIR_AUDIOREC,nId);
    if(rWavWriter.Create(cFilePath,nSamplerateHz,nChannels,nBitsPerSample))
      bSuccess=true;
    else
      m_mapAudioRec.erase(nId);
    }
  return bSuccess;
  }



//--------------------------------------------------------------------------
// OnAudioRecData()        gets called when client has audio recording data
// ----------------
// Input: nId = general purpose ID the server wanted to be reflected by the client
// Input: pSampleData = data. Number of entities must be ChannelCount * nSamples
//                      if multiple channels (n): first n data are the first samples of n channels and so on
//        nSamples = number of samples (of all channels)
// Return: 

bool CMyFakeLogger::OnAudioRecData (int nId, const int *pSampleData, int nSamples)
  {
  bool bSuccess=false;
  Abk::CAbkSingleLock guardAudioRec(&m_mutexAudioRec,true);
  AUDIOREC_MAP::iterator itAudioRec=m_mapAudioRec.find(nId);
  if(itAudioRec!=m_mapAudioRec.end())
    {
    CMyWavFileWriter &rWavWriter=itAudioRec->second;
    bSuccess=rWavWriter.WriteData(pSampleData,nSamples);
    }
  return bSuccess;
  }



//--------------------------------------------------------------------------
// OnAudioRecFooter()      gets called when client terminates audio recording
// ------------------
// Input: nId = general purpose ID the server wanted to be reflected by the client
// Return: 

bool CMyFakeLogger::OnAudioRecFooter (int nId)
  {
  bool bSuccess=false;
  Abk::CAbkSingleLock guardAudioRec(&m_mutexAudioRec,true);
  AUDIOREC_MAP::iterator itAudioRec=m_mapAudioRec.find(nId);
  if(itAudioRec!=m_mapAudioRec.end())
    {
    CMyWavFileWriter &rWavWriter=itAudioRec->second;
    bSuccess=rWavWriter.Close();
    m_mapAudioRec.erase(nId);
    }
  return bSuccess;
  }



