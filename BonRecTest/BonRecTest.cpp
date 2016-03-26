#include "stdafx.h"
#include "BonRecTest.h"

CBonRecTest::CBonRecTest() :
	bonDriverPath(NULL),
	outputPath(NULL),
	space(0UL),
	channel(0L),
	hBonDriver(NULL),
	pBonDriver(NULL),
	pBonDriver2(NULL),
	hRecThread(NULL),
	hOutput(INVALID_HANDLE_VALUE),
	isThreadWorking(false)
{
}

CBonRecTest::~CBonRecTest()
{
	Stop();
}

bool CBonRecTest::Start()
{
	try {
		LoadBonDriver();
		OpenOutput();
		OpenTuner();
		StartThread();
	}
	catch (LPCTSTR err) {
		fprintf(stderr, "Error: %S\n", err);

		return false;
	}

	return true;
}

bool CBonRecTest::Stop()
{
	try {
		StopThread();
		CloseTuner();
		CloseOutput();
		UnloadBonDriver();
	}
	catch (LPCTSTR err) {
		fprintf(stderr, "Error: %S\n", err);

		return false;
	}

	return true;
}

void CBonRecTest::LoadBonDriver()
{
	std::cerr << "Load BonDriver..." << std::endl;

	if (hBonDriver) {
		throw TEXT("BonDriver is already loaded");
	}

	if (!bonDriverPath) {
		throw TEXT("BonDriver path is invalid");
	}

	hBonDriver = LoadLibrary(bonDriverPath);

	if (!hBonDriver) {
		throw TEXT("Could not load BonDriver");
	}
}

void CBonRecTest::UnloadBonDriver()
{
	if (!hBonDriver) return;

	std::cerr << "Unload BonDriver..." << std::endl;

	FreeLibrary(hBonDriver);
	hBonDriver = NULL;
}

void CBonRecTest::OpenTuner()
{
	std::cerr << "Open Tuner..." << std::endl;

	if (pBonDriver) {
		throw TEXT("BonDriver is already opened");
	}

	if (!hBonDriver) {
		throw TEXT("BonDriver is not loaded");
	}

	IBonDriver* (*CreateBonDriver)();
	CreateBonDriver = (IBonDriver* (*)())GetProcAddress(hBonDriver, "CreateBonDriver");

	if (!CreateBonDriver) {
		throw TEXT("Could not get address CreateBonDriver()");
	}

	pBonDriver = CreateBonDriver();

	if (!pBonDriver) {
		throw TEXT("Could not get IBonDriver");
	}

	pBonDriver2 = dynamic_cast<IBonDriver2 *>(pBonDriver);

	if (!pBonDriver->OpenTuner() || !pBonDriver2->SetChannel(space, channel)) {
		pBonDriver->Release();

		pBonDriver = NULL;
		pBonDriver2 = NULL;

		throw TEXT("Could not open tuner");
	}

	
}

void CBonRecTest::CloseTuner()
{
	if (!pBonDriver) return;

	std::cerr << "Close Tuner..." << std::endl;

	pBonDriver->CloseTuner();
	pBonDriver->Release();

	pBonDriver = NULL;
	pBonDriver2 = NULL;
}

void CBonRecTest::OpenOutput()
{
	std::cerr << "Open Output..." << std::endl;

	if (hOutput != INVALID_HANDLE_VALUE) {
		throw TEXT("Output already exists");
	}

	if (outputPath == NULL) {
		throw TEXT("Output path is invalid");
	}

	if (_tcscmp(outputPath, TEXT("-"))) {
		hOutput = CreateFile(outputPath, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

		if (hOutput == INVALID_HANDLE_VALUE) {
			throw TEXT("Could not open output");
		}
	}
	else {
		hOutput = GetStdHandle(STD_OUTPUT_HANDLE);
	}
}

void CBonRecTest::CloseOutput()
{
	if (hOutput == INVALID_HANDLE_VALUE) return;

	std::cerr << "Close Output..." << std::endl;

	if (!CloseHandle(hOutput)) {
		throw TEXT("Could not close output");
	}

	hOutput = INVALID_HANDLE_VALUE;
}

void CBonRecTest::StartThread()
{
	std::cerr << "Start Thread..." << std::endl;

	isThreadWorking = true;

	hRecThread = (HANDLE)_beginthreadex(NULL, 0, RecThread, this, 0, NULL);

	if (!hRecThread) {
		isThreadWorking = false;

		throw TEXT("Could not start thread");
	}
}

void CBonRecTest::StopThread()
{
	if (!hRecThread) return;

	std::cerr << "Stop Thread..." << std::endl;

	isThreadWorking = false;

	if (WaitForSingleObject(hRecThread, 5000UL) != WAIT_OBJECT_0) {
		std::cerr << "Terminate thread!" << std::endl;

		TerminateThread(hRecThread, -1);
	}

	CloseHandle(hRecThread);
	hRecThread = NULL;
}


unsigned int __stdcall CBonRecTest::RecThread(void *pParam)
{
	CBonRecTest *pThis = static_cast<CBonRecTest *>(pParam);

	pThis->RecMain();

	return 0;
}

void CBonRecTest::RecMain()
{
	BYTE *pStreamData = NULL;
	DWORD streamSize = 0UL;
	DWORD streamRemain = 0UL;
	DWORD bytesWritten = 0UL;
	DWORD bytesRead = 0UL;

	while (isThreadWorking) {
		if (pBonDriver->GetTsStream(&pStreamData, &streamSize, &streamRemain)) {
			if (pStreamData && streamSize) {
				WriteFile(hOutput, pStreamData, streamSize, &bytesWritten, NULL);
			}
		}

		bytesRead += streamSize;

		if (bytesRead > 188 * 256 * 256) {
			std::cerr << "Signal: " << pBonDriver->GetSignalLevel() << "dB" << std::endl;
			bytesRead = 0;
		}

		Sleep(streamRemain ? 0UL : 10UL);
	}
}
