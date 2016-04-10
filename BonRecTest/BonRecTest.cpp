#include "stdafx.h"
#include "BonRecTest.h"

CBonRecTest::CBonRecTest() :
	bonDriverPath(NULL),
	decoderPath(NULL),
	outputPath(NULL),
	space(0UL),
	channel(0L),
	emm(false),
	log(false),
	hBonDriver(NULL),
	hDecoder(NULL),
	pBonDriver2(NULL),
	pDecoder2(NULL),
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
		LoadDecoder();
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
		UnloadDecoder();
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
	if (log) std::cerr << "Load BonDriver..." << std::endl;

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

	IBonDriver2* (*CreateBonDriver)();
	CreateBonDriver = (IBonDriver2* (*)())GetProcAddress(hBonDriver, "CreateBonDriver");

	if (!CreateBonDriver) {
		FreeLibrary(hBonDriver);
		hBonDriver = NULL;

		throw TEXT("Could not get address CreateBonDriver()");
	}

	pBonDriver2 = CreateBonDriver();

	if (!pBonDriver2) {
		FreeLibrary(hBonDriver);
		hBonDriver = NULL;

		throw TEXT("Could not get IBonDriver");
	}
}

void CBonRecTest::UnloadBonDriver()
{
	if (!hBonDriver) return;

	if (log) std::cerr << "Unload BonDriver..." << std::endl;

	pBonDriver2->Release();
	pBonDriver2 = NULL;

	FreeLibrary(hBonDriver);
	hBonDriver = NULL;
}

void CBonRecTest::LoadDecoder()
{
	if (!decoderPath) return;

	if (log) std::cerr << "Load Decoder..." << std::endl;

	if (hDecoder) {
		throw TEXT("Decoder is already loaded");
	}

	hDecoder = LoadLibrary(decoderPath);

	if (!hDecoder) {
		throw TEXT("Could not load Decoder");
	}

	IB25Decoder2* (*CreateB25Decoder)();
	CreateB25Decoder = (IB25Decoder2* (*)())GetProcAddress(hDecoder, "CreateB25Decoder");

	if (!CreateB25Decoder) {
		FreeLibrary(hDecoder);
		hDecoder = NULL;

		throw TEXT("Could not get address CreateB25Decoder()");
	}

	pDecoder2 = CreateB25Decoder();

	if (!pDecoder2) {
		FreeLibrary(hDecoder);
		hDecoder = NULL;

		throw TEXT("Could not get IB25Decoder");
	}

	if (!pDecoder2->Initialize()) {
		FreeLibrary(hDecoder);
		hDecoder = NULL;

		throw TEXT("Could not initialize IB25Decoder");
	}

	pDecoder2->DiscardNullPacket(true);
	pDecoder2->DiscardScramblePacket(false);
	pDecoder2->EnableEmmProcess(emm);
}

void CBonRecTest::UnloadDecoder()
{
	if (!hDecoder) return;

	if (log) std::cerr << "Unload Decoder..." << std::endl;

	pDecoder2->Release();
	pDecoder2 = NULL;

	FreeLibrary(hDecoder);
	hDecoder = NULL;
}

void CBonRecTest::OpenOutput()
{
	if (log) std::cerr << "Open Output..." << std::endl;

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

	if (log) std::cerr << "Close Output..." << std::endl;

	if (!CloseHandle(hOutput)) {
		throw TEXT("Could not close output");
	}

	hOutput = INVALID_HANDLE_VALUE;
}

void CBonRecTest::OpenTuner()
{
	if (log) std::cerr << "Open Tuner..." << std::endl;

	if (!pBonDriver2->OpenTuner()) {
		throw TEXT("Could not open tuner");
	}

	if (!pBonDriver2->SetChannel(space, channel)) {
		pBonDriver2->CloseTuner();

		throw TEXT("Could not set channel");
	}
}

void CBonRecTest::CloseTuner()
{
	if (!hBonDriver) return;

	if (log) std::cerr << "Close Tuner..." << std::endl;

	pBonDriver2->CloseTuner();
}

void CBonRecTest::StartThread()
{
	if (log) std::cerr << "Start Thread..." << std::endl;

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

	if (log) std::cerr << "Stop Thread..." << std::endl;

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
	BYTE *pDecodeData = NULL;
	DWORD decodeSize = 0UL;
	DWORD bytesWritten = 0UL;
	DWORD bytesRead = 0UL;

	while (isThreadWorking) {
		if (pBonDriver2->GetTsStream(&pStreamData, &streamSize, &streamRemain)) {
			if (pStreamData && streamSize) {
				if (hDecoder && pDecoder2->Decode(pStreamData, streamSize, &pDecodeData, &decodeSize)) {
					WriteFile(hOutput, pDecodeData, decodeSize, &bytesWritten, NULL);
				}
				else {
					WriteFile(hOutput, pStreamData, streamSize, &bytesWritten, NULL);
				}
			}
		}

		bytesRead += streamSize;

		if (bytesRead > 188 * 256 * 256) {
			if (log) std::cerr << "Signal: " << pBonDriver2->GetSignalLevel() << "dB" << std::endl;
			bytesRead = 0;
		}

		Sleep(streamRemain ? 0UL : 10UL);
	}
}
