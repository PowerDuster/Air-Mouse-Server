#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <WinSock2.h>
#include <thread>

#pragma comment(lib, "Ws2_32.lib")

#define PORT 6666
#define KEY_PORT 6667
#define PROBE_RESPONSE_PORT 7777
#define COMPATIBILITY_CODE "4"

#define MOUSE_MOVE -9
#define KB_INPUT -10


//	TODO: Add an interactive GUI
//	TODO: Allow adding macros
//	TODO: Add an interrupt handler for exit
//	TODO: Minimize to system tray

const int SCREEN_WIDTH = GetSystemMetrics(SM_CXVIRTUALSCREEN);
const int SCREEN_HEIGHT = GetSystemMetrics(SM_CYVIRTUALSCREEN);

BOOL isRunAsAdministrator() {
	BOOL fIsRunAsAdmin = FALSE;
	DWORD dwError = ERROR_SUCCESS;
	PSID pAdministratorsGroup = NULL;

	SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
	if (!AllocateAndInitializeSid(&NtAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &pAdministratorsGroup)) {
		dwError = GetLastError();
		goto Cleanup;
	}

	if (!CheckTokenMembership(NULL, pAdministratorsGroup, &fIsRunAsAdmin)) {
		dwError = GetLastError();
		goto Cleanup;
	}

Cleanup:
	if (pAdministratorsGroup) {
		FreeSid(pAdministratorsGroup);
		pAdministratorsGroup = NULL;
	}
	if (ERROR_SUCCESS != dwError) {
		throw dwError;
	}
	return fIsRunAsAdmin;
}

void MouseSetup(INPUT *buffer) {
	buffer->type = INPUT_MOUSE;
	buffer->mi.dx = 0;
	buffer->mi.dy = 0;
	buffer->mi.mouseData = 0;
	buffer->mi.dwFlags = MOUSEEVENTF_ABSOLUTE;
	buffer->mi.time = 0;
	buffer->mi.dwExtraInfo = 0;
}

void KeyboardSetup(INPUT *buffer) {
	buffer->type = INPUT_KEYBOARD;
	buffer->ki.dwFlags = 0;
	buffer->ki.wScan = 0;
	buffer->ki.time = 0;
	buffer->ki.dwExtraInfo = 0;
}

void displayMessage(LPCWSTR title, LPCWSTR message, long icon) {
	MessageBox(NULL, message, title, icon);
}

void ServerThread() {
	SOCKET broadcastListenerSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);		//IPPROTO_UDP  (17)  can use 0 to let driver(?) decide
	if (broadcastListenerSocket == INVALID_SOCKET) {
		return;
	}
	sockaddr_in listen_addr;
	listen_addr.sin_family = AF_INET;
	listen_addr.sin_port = htons(PROBE_RESPONSE_PORT);
	listen_addr.sin_addr.s_addr = htonl(INADDR_ANY);	// INADDR_ANY=0.0.0.0
	if (bind(broadcastListenerSocket, (SOCKADDR*)&listen_addr, sizeof(listen_addr))) {
		closesocket(broadcastListenerSocket);
		return;
	}
	sockaddr_in client_addr;
	int caddr_size = sizeof(client_addr);
	int size = 0;
	char recvBuf[4];
	memset(recvBuf, 0, 4);
	char ver[5] = COMPATIBILITY_CODE;

	struct sockaddr_in broadcastAddr;	//	Can reuse listen_addr??
	memset(&broadcastAddr, 0, sizeof(broadcastAddr));
	broadcastAddr.sin_family = AF_INET;
	broadcastAddr.sin_addr.s_addr = inet_addr("255.255.255.255");
	broadcastAddr.sin_port = htons(PROBE_RESPONSE_PORT);
	SOCKET announceSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	setsockopt(announceSocket, SOL_SOCKET, SO_BROADCAST, "1", 1);
	//printf("string length: %d\n", strlen(ver));
	sendto(announceSocket, ver, strlen(ver), 0, (SOCKADDR*)&broadcastAddr, sizeof(broadcastAddr));
	//closesocket(announceSocket);	// Cannot close!
	while ((size = recvfrom(broadcastListenerSocket, recvBuf, 4, 0, (SOCKADDR*)&client_addr, &caddr_size)) != SOCKET_ERROR) {
		if (size > 1 && recvBuf[0] == 'v') {
			//printf("\nIF: %s  %d\n", recvBuf, size);
			int clientCode = atoi(&recvBuf[1]);
			int localCode = atoi(ver);
			if (clientCode > localCode) {
				std::thread msg_thread(displayMessage, L"Desktop application outdated",
					L"To get latest features and avoid compatibility issues, download the latest version of this application from:\nhttps://goo.gl/m233tM", MB_ICONWARNING);
				msg_thread.detach();
			}
			else if (clientCode < localCode) {
				std::thread msg_thread(displayMessage, L"Android application outdated",
					L"Please update the app to the latest version from the PlayStore to avoid compatibility issues.", MB_ICONWARNING);
				msg_thread.detach();
			}
		}
		else {
			//printf("\nELSE: %d  %d\n", recvBuf[0], size);
			//printf(inet_ntoa(client_addr.sin_addr));
			//printf("string length: %d\n", strlen(ver));
			sendto(broadcastListenerSocket, ver, strlen(ver), 0, (SOCKADDR*)&client_addr, sizeof(client_addr));
		}
		memset(&client_addr, 0, caddr_size);
	}
	closesocket(broadcastListenerSocket);
}


const size_t inputSize = sizeof(INPUT);


void KeyInputThread() {
	SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sock == INVALID_SOCKET) {
		exit;
	}
	sockaddr_in bindaddr;
	bindaddr.sin_family = AF_INET;
	bindaddr.sin_port = htons(KEY_PORT);
	bindaddr.sin_addr.s_addr = inet_addr("0.0.0.0");
	if (bind(sock, (SOCKADDR*)&bindaddr, sizeof(bindaddr))) {
		closesocket(sock);
		return;
	}

	INPUT* inputArr[10];
	int sizes[sizeof(inputArr) / sizeof(inputArr[0])];
	//INPUT **inputArr = (INPUT **)malloc(sizeof(INPUT*) * INPUT_COUNT);
	//printf("Sizes: %d %d\n", sizeof(inputArr), sizeof(inputArr)/ sizeof(inputArr[0]));
	for (int i = 0; i < sizeof(inputArr) / sizeof(inputArr[0]); i++) {
		inputArr[i] = (INPUT*)malloc(inputSize);
		sizes[i] = 1;
		MouseSetup(inputArr[i]);
	}

	/* Left MB Pressed */
	inputArr[0]->mi.dwFlags = (MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_LEFTDOWN);

	/* Left MB Released */
	inputArr[1]->mi.dwFlags = (MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_LEFTUP);

	/* Right MB Pressed */
	inputArr[2]->mi.dwFlags = (MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_RIGHTDOWN);

	/* Right MB Released */
	inputArr[3]->mi.dwFlags = (MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_RIGHTUP);

	/* Wheel Down 1 click */
	inputArr[4]->mi.dwFlags = MOUSEEVENTF_WHEEL;
	inputArr[4]->mi.mouseData = -WHEEL_DELTA;	 // WHEEL_DELTA=120 (One wheel click)

	/* Wheel Up 1 click */
	inputArr[5]->mi.dwFlags = MOUSEEVENTF_WHEEL;
	inputArr[5]->mi.mouseData = WHEEL_DELTA;

	/* Left click */
	sizes[6] = 2;
	inputArr[6] = (INPUT*)realloc(inputArr[6], inputSize * 2);
	MouseSetup(&inputArr[6][0]);
	MouseSetup(&inputArr[6][1]);
	inputArr[6][0] = *inputArr[0];
	inputArr[6][1] = *inputArr[1];

	/* Right click */
	sizes[7] = 2;
	inputArr[7] = (INPUT*)realloc(inputArr[7], inputSize * 2);
	MouseSetup(&inputArr[7][0]);
	MouseSetup(&inputArr[7][1]);
	inputArr[7][0] = *inputArr[2];
	inputArr[7][1] = *inputArr[3];

	/* Double click */
	sizes[8] = 4;
	inputArr[8] = (INPUT*)realloc(inputArr[8], inputSize * 4);
	MouseSetup(&inputArr[8][0]);
	MouseSetup(&inputArr[8][1]);
	MouseSetup(&inputArr[8][2]);
	MouseSetup(&inputArr[8][3]);
	inputArr[8][0] = *inputArr[0];
	inputArr[8][1] = *inputArr[1];
	inputArr[8][2] = *inputArr[0];
	inputArr[8][3] = *inputArr[1];

	/* Move mouse to center */
	inputArr[9]->mi.dwFlags = (MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE);
	inputArr[9]->mi.dx = 32768;
	inputArr[9]->mi.dy = 32768;


	INPUT kbInput;
	KeyboardSetup(&kbInput);

	/* Keyboard Character Input */
	INPUT kbInputChar;
	kbInputChar.type = INPUT_KEYBOARD;
	kbInputChar.ki.dwFlags = KEYEVENTF_UNICODE;
	kbInputChar.ki.time = 0;
	kbInputChar.ki.dwExtraInfo = 0;
	kbInputChar.ki.wVk = 0;

	INPUT mInput;
	MouseSetup(&mInput);
	mInput.mi.dwFlags = (MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE);

	size_t size;
	size_t i = 0;
	char buff[128];

	while ((size = recv(sock, buff, 128, 0)) != SOCKET_ERROR) {
		/*if (size == 1) {	// For quickness maybe
			SendInput(sizes[buff[0]], inputArr[buff[0]], inputSize);
			continue;
		}*/
		while (i < size) {
			if (buff[i] == KB_INPUT && (i + 2) < size) {
				if (buff[i + 1] == KEYEVENTF_UNICODE) {
					kbInput.ki.dwFlags = KEYEVENTF_UNICODE;
					kbInput.ki.wVk = 0;
					kbInput.ki.wScan = buff[i + 2];
				}
				else {
					kbInput.ki.dwFlags = buff[i + 1];
					kbInput.ki.wVk = ((uint8_t)buff[i + 2]);
				}
				SendInput(1, &kbInput, inputSize);
				i += 3;
			}
			else if (buff[i] == MOUSE_MOVE && (i + 2) < size) {
				mInput.mi.dx = (((uint8_t)buff[i + 1]) / 256.0) * 65535;
				mInput.mi.dy = (((uint8_t)buff[i + 2]) / 256.0) * 65535;
				SendInput(1, &mInput, inputSize);
				i += 3;
			}
			else if(buff[i] >= 0 && buff[i] < 10) {
				SendInput(sizes[buff[i]], inputArr[buff[i]], inputSize);
				i++;
			}
			else {
				break;
			}
		}
		i = 0;
	}
}




void main() {
	//ShowWindow(FindWindowA("ConsoleWindowClass", NULL), false);
	WSADATA wsadata;
	if (WSAStartup(MAKEWORD(2, 2), &wsadata) != 0) {
		exit;
	}

	std::thread st(ServerThread);
	st.detach();

	SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sock == INVALID_SOCKET) {
		exit;
	}
	sockaddr_in bindaddr;
	bindaddr.sin_family = AF_INET;
	bindaddr.sin_port = htons(PORT);
	bindaddr.sin_addr.s_addr = inet_addr("0.0.0.0");
	if (bind(sock, (SOCKADDR*)&bindaddr, sizeof(bindaddr))) {
		displayMessage(L"Bind error\n", L"Another instance of the application might be running.", MB_ICONWARNING);
		closesocket(sock);
		exit;
	}

	std::thread kIt(KeyInputThread);
	kIt.detach();

	if (!isRunAsAdministrator()) {
		//std::thread msg_thread(displayMessage, L"Running as non-elevated user\n",
		//	L"Application will lose control over elevated processes such as Task Manager.\nRun the application as Administrator to fix this.", MB_ICONINFORMATION);
		////if(msg_thread.joinable()) {
		//msg_thread.detach();
		printf("Running as non-elevated user, the application won't control mouse over some processes like Task Manager.\n\nRun as Administrator to fix this.\n");
	}

	INPUT mouse;
	MouseSetup(&mouse);
	mouse.mi.dwFlags = MOUSEEVENTF_MOVE;

	//atexit();

	//printf("%d\n", (uint8_t)-12);

	size_t size;
	char buff[8];
	float x = 0, y = 0;

	while ((size = recv(sock, buff, 8, 0)) != SOCKET_ERROR) {
		memcpy(&x, &buff, 4);
		memcpy(&y, buff + 4, 4);
		mouse.mi.dx = roundf(x * SCREEN_WIDTH);
		mouse.mi.dy = roundf(y * SCREEN_HEIGHT);
		SendInput(1, &mouse, inputSize);
		//printf("%d  %f  %f\n", size, x, y);
		//memset(buff, 0, 64);
	}
	/*for (int i = 0; i < 14; i++) {
	free(inputArr[i]);
	}
	free(inputArr);*/
	WSACleanup();
}
