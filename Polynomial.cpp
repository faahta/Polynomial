/*
    - random file access (in R and W)
 *  - 2 barriers
 *  - election strategy
 */

#define UNICODE
#define _UNICODE
#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <tchar.h>
#include <process.h>
#include <stdio.h>

#define DEBUG 0

 /*
  * Select thread calls as:
  * _beginthreadex IFF 1
  * createThread IFF 0
  */
#define THREAD_CALL 0

  // Global structures
typedef struct threads {
	INT id;
	LPTSTR name;
	INT degree;
	INT firstThread;
	FLOAT var, coef;
	FLOAT term;
	FLOAT result;
} threads_t;

typedef struct counter {
	INT count;  // Counter for Barrier
	HANDLE mt;  // Mutex for Barrier
} counter_t;

// Global variable
threads_t* threadData;
HANDLE barrier1;  // Semaphore for Barrier 1
HANDLE barrier2;  // Semaphore for Barrier 2
counter_t* counter;

void set_overlapped(OVERLAPPED*, DWORD);
void filePrint(LPCTSTR);
void init(DWORD);
DWORD WINAPI threadFunction(LPVOID);



int _tmain(int argc, LPTSTR argv[]) {
	DWORD n, nIn;
	INT i;

	// Read Polynomial Degree
	HANDLE hIn;
	hIn = CreateFile(argv[1], GENERIC_READ,
		FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL, NULL);
	if (hIn == INVALID_HANDLE_VALUE) {
		_tprintf(_T("Open file error: %x\n"), GetLastError());
		return 2;
	}
	ReadFile(hIn, &n, sizeof(INT), &nIn, NULL);
	CloseHandle(hIn);

	init(n);

	HANDLE* hThreads = (HANDLE*)malloc(n * sizeof(HANDLE));
	threadData = (threads_t*)malloc(n * sizeof(threads_t));
	DWORD* threadId = (DWORD*)malloc(n*sizeof(DWORD));\

	
	for (i = 0; i < n; i++) {
		threadData[i].id = i + 1;
		threadData[i].degree = n;
		threadData[i].name = argv[1];
		//_tprintf(_T("creating thread %d\n"), threadData[i].id);
		hThreads[i] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)threadFunction, &threadData[i].id, 0, &threadId[i]);
		if (hThreads[i] == NULL) {
			_tprintf(_T("invalid handle\n"));
			ExitProcess(0);
		}
	}
	WaitForMultipleObjects(n, hThreads, TRUE, INFINITE);
	for (i = 0; i < n; i++) {
		CloseHandle(hThreads[i]);
	}
	_tprintf(_T("Output File (debug printing):\n"));
	filePrint(argv[1]);
	return 0;
}

DWORD WINAPI threadFunction(LPVOID lpParam) {
	INT* th = (INT*)lpParam;
	INT id = *th - 1;
	OVERLAPPED ov = { 0, 0, 0, 0, NULL };
	HANDLE hIn;
	hIn = CreateFile(threadData[id].name, GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hIn == INVALID_HANDLE_VALUE) {
		_tprintf(_T("open file error\n"));
		return 2;
	}
	INT i, row = 0;  DWORD v, c, c0, r, nIn;
	_tprintf(_T("thread %d\n"),id+1);
	while (1) {
		v = row * threadData[id].degree * 2;
		set_overlapped(&ov, v);
		ReadFile(hIn, &threadData[id].var, sizeof(FLOAT), &nIn, &ov);
		if (nIn == 0) { break; }
		
		/*Read variable*/
		v = row * threadData[id].degree * 2;
		set_overlapped(&ov, v);
		ReadFile(hIn, &threadData[id].var, sizeof(FLOAT), &nIn, &ov);

		/*Read coefficient*/
		c = v + (threadData[id].id + 1);
		set_overlapped(&ov, c);
		ReadFile(hIn, &threadData[id].coef, sizeof(FLOAT), &nIn, &ov);

		/*Compute term*/
		threadData[id].term = 1;
		for (i = 0; i < threadData[id].id; i++) {
			threadData[id].term = threadData[id].term * threadData[id].var;
		}
		threadData[id].term = threadData[id].term * threadData[id].coef;
#if DEBUG
		_tprintf(_T("Thread %d read var=%f coeff=%f term=%f reached barrier\n"),
			threadData[id].id, threadData[id].var,
			threadData[id].coef, threadData[id].term);
#endif
		/*Barrier 1*/
		WaitForSingleObject(counter->mt, INFINITE);
			counter->count++;
			if (counter->count == 1){
				/*fastest thread*/
				threadData[id].firstThread = 1;
			} else {
				threadData[id].firstThread = 0;
			}
			if (counter->count == threadData[id].degree) {
				for (i = 0; i < threadData[id].degree; i++)
					ReleaseSemaphore(barrier1, 1, NULL);
			}
		ReleaseMutex(counter->mt);

		WaitForSingleObject(barrier1, INFINITE);
		if (threadData[id].firstThread == 1) {
			/*Read constant coefficient c0*/
			c0 = v + 1;
			set_overlapped(&ov, c0);
			ReadFile(hIn, &threadData[id].result, sizeof(FLOAT), &nIn, &ov);
			/*compute final sum*/
			for (i = 0; i < threadData[id].degree; i++) {
				threadData[id].result += threadData[i].term;
			}
#if DEBUG
			_tprintf(_T("I'm the first one -> Thread %d Sum=%f\n"), threadData[id].id, threadData[id].result);
#endif
			/* write result to file*/
			r = (row + 1) * (threadData[id].degree) * 2 - 1;
			set_overlapped(&ov, r);
			WriteFile(hIn, &threadData[id].result, sizeof(FLOAT), &nIn, &ov);


		}

		/*Barrier 2*/
		WaitForSingleObject(counter->mt, INFINITE);
			counter->count--;
			if (counter->count == 0) {
				for (i = 0; i < threadData[id].degree; i++)
					ReleaseSemaphore(barrier2, 1, NULL);
			}
		ReleaseMutex(counter->mt);

		WaitForSingleObject(barrier2, INFINITE);
		row++;
	}

	CloseHandle(hIn);
	ExitThread(0);
}

void init(DWORD degree) {
	counter = (counter_t*)malloc(sizeof(counter_t));
	counter->count = 0;
	counter->mt = CreateMutex(NULL, FALSE, NULL);
	barrier1 = CreateSemaphore(NULL, 0, degree, NULL);
	barrier2 = CreateSemaphore(NULL, 0, degree, NULL);
}

void set_overlapped(OVERLAPPED* ov, DWORD n) {
	LARGE_INTEGER filePos;

	filePos.QuadPart = sizeof(INT) + n * sizeof(FLOAT);
	ov->Offset = filePos.LowPart;
	ov->OffsetHigh = filePos.HighPart;
	ov->hEvent = 0;

	return;
}

void filePrint(LPCTSTR name) {
	HANDLE hIn;
	FLOAT f;
	INT i, n;
	DWORD nIn;

	hIn = CreateFile(name, GENERIC_READ, FILE_SHARE_READ, NULL,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hIn == INVALID_HANDLE_VALUE) {
		_tprintf(_T("Cannot open input file. Error: %x\n"),
			GetLastError());
		exit(1);
	}

	ReadFile(hIn, &n, sizeof(INT), &nIn, NULL);
	_tprintf(_T("%d\n"), n);
	i = 0;
	while (ReadFile(hIn, &f, sizeof(FLOAT), &nIn, NULL) && nIn > 0) {
		_tprintf(_T("%f "), f);
		if ((++i) == 6) {
			_tprintf(_T("\n"));
			i = 0;
		}
	}
	CloseHandle(hIn);

	return;
}
