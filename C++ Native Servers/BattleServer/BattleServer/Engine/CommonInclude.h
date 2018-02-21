#pragma once

#include <Windows.h>
#include <Process.h>
#include <iostream>
#include <time.h>
#include <strsafe.h>
#include <conio.h>  

#include <signal.h>
#include "Queue.h"
#include "CProfiler.h"
#include "CSystemLog.h"
#include "CLockFreeQueue.h"
#include "CLockFreeStack.h"
#include "PacketBuffer.h"
#include "StreamBuffer.h"
#include "CCrashDump.h"


#ifdef PROFILE_CHECK
#define PRO_BEGIN(x)  CProfile::GetInstance()->ProfileBegin(x)
#define PRO_END(x)  CProfile::GetInstance()->ProfileEnd(x)
#define PRO_TEXT(x)	CProfile::GetInstance()->ProfileOutText(x)
#else
#define PRO_BEGIN(x)
#define PRO_END(x)
#define PRO_TEXT(x)	
#endif

