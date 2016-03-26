#pragma once

class CBonRecTest
{
public:
	CBonRecTest();
	~CBonRecTest();
	bool Start();
	bool Stop();

	LPCTSTR bonDriverPath;
	LPCTSTR outputPath;
	DWORD space;
	DWORD channel;

private:
	void LoadBonDriver();
	void UnloadBonDriver();
	void OpenTuner();
	void CloseTuner();
	void OpenOutput();
	void CloseOutput();
	void StartThread();
	void StopThread();

	static unsigned int __stdcall RecThread(void *param);
	void RecMain();

	HMODULE hBonDriver;
	IBonDriver *pBonDriver;
	IBonDriver2 *pBonDriver2;
	HANDLE hRecThread;
	HANDLE hOutput;
	bool isThreadWorking;
};
