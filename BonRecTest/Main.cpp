// BonRecTest.cpp : コンソール アプリケーションのエントリ ポイントを定義します。
//

#include "stdafx.h"
#include "BonRecTest.h"


bool finish = false;

void hSignal(int signal)
{
	finish = true;
}

void usage()
{
	std::cerr << "Usage: BonRecTest.exe --driver BonDriver.dll --output output.ts [--space space] [--channel channel]" << std::endl;
}

int _tmain(int argc, TCHAR* argv[])
{
	std::cerr << "* BonRecTest *" << std::endl;

	if (argc < 2) {
		usage();

		return 0;
	}

	CBonRecTest pBonRecTest;

	TCHAR *pEnd;
	bool used = false;

	for (int i = 1; i < argc - 1; i++) {
		if (used) {
			used = false;
			continue;
		}

		if (!_tcscmp(argv[i], TEXT("--driver"))) {
			pBonRecTest.bonDriverPath = argv[i + 1];
			used = true;
		}
		else if (!_tcscmp(argv[i], TEXT("--decoder"))) {
			pBonRecTest.decoderPath = argv[i + 1];
			used = true;
		}
		else if (!_tcscmp(argv[i], TEXT("--output"))) {
			pBonRecTest.outputPath = argv[i + 1];
			used = true;
		}
		else if (!_tcscmp(argv[i], TEXT("--space"))) {
			pBonRecTest.space = _tcstoul(argv[i + 1], &pEnd, 10);
			used = true;
		}
		else if (!_tcscmp(argv[i], TEXT("--channel"))) {
			DWORD channel = _tcstoul(argv[i + 1], &pEnd, 10);

			if (channel < 10000) {
				pBonRecTest.channel = channel;
			}
			else {
				pBonRecTest.space = channel / 10000UL;
				pBonRecTest.channel = channel % 10000UL;
			}
			used = true;
		}
		else if (!_tcscmp(argv[i], TEXT("--emm"))) {
			pBonRecTest.emm = true;
		}
	}

	if (!pBonRecTest.Start()) {
		return -1;
	}

	if (signal(SIGTERM, hSignal) == SIG_ERR || signal(SIGINT, hSignal) == SIG_ERR) {
		std::cerr << "Error: Could not regist signal handler" << std::endl;

		return -1;
	}

	while (!finish)
	{
		Sleep(10UL);
	}

	std::cerr << "Recieve signal!" << std::endl;

    return 0;
}

