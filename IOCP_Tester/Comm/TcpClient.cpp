#pragma warning (disable:4996)

#include "TcpClient.h"

TcpClient::TcpClient(int bufSize)
{
	m_connected = false;
	m_pRecvBuf = make_unique<CircularBuf>(bufSize);

	WSADATA	wsaData;

	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		OutputDebugStringA("WSA init fail\n");
	}
}


TcpClient::~TcpClient()
{
	Disconnect();
}

void TcpClient::StopRecv()
{
	m_connected = false;
}


bool TcpClient::CreateSocket(string server_ip, int serverPort, int localPort)
{
	bool returnVal = true;
	if (!m_connected)
	{
		m_clientSocket.socket = socket(PF_INET, SOCK_STREAM, 0);

		if (m_clientSocket.socket == (SOCKET)SOCKET_ERROR)
		{
			returnVal = false;
		}
		else
		{
			memset(&m_clientSocket.socketAddr, NULL, sizeof(m_clientSocket.socketAddr));

			// 로컬포트 지정 시 작업
			if (localPort != 0)
			{
				m_clientSocket.socketAddr.sin_family = AF_INET;
				m_clientSocket.socketAddr.sin_addr.S_un.S_addr = INADDR_ANY;
				m_clientSocket.socketAddr.sin_port = htons(localPort);

				if (bind(m_clientSocket.socket, (SOCKADDR*)&m_clientSocket.socketAddr, sizeof(m_clientSocket.socketAddr)) == SOCKET_ERROR)
				{
					OutputDebugStringA("local port set error\n");
					return false;
				}
			}

			// keep alive 설정
			char optval = 1;
			setsockopt(m_clientSocket.socket, IPPROTO_TCP, SO_KEEPALIVE, &optval, sizeof(optval));		// keep alive 설정

			tcp_keepalive ka_settings;
			ka_settings.onoff = 1;
			ka_settings.keepalivetime = 10000;      // 10초
			ka_settings.keepaliveinterval = 3000;   // 3초

			DWORD bytesReturned;
			if (WSAIoctl(m_clientSocket.socket, SIO_KEEPALIVE_VALS, &ka_settings, sizeof(ka_settings), NULL, 0, &bytesReturned, NULL, NULL) == SOCKET_ERROR)
			{
				printf("client socket set SIO_KEEPALIVE_VALS failed: %d\n", WSAGetLastError());
				closesocket(m_clientSocket.socket);
				returnVal = false;
			}
			else
			{
				// 서버 주소 지정
				SOCKADDR_IN serverAddr;
				serverAddr.sin_family = AF_INET;
				serverAddr.sin_port = htons(serverPort);
				serverAddr.sin_addr.s_addr = inet_addr(server_ip.c_str());

				// 서버 연결
				if (connect(m_clientSocket.socket, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
				{
					printf("server connect fail\n");
					closesocket(m_clientSocket.socket);
					returnVal = false;
				}
				else
				{
					// 수신 스레드 동작 확인
					if (!m_worker.joinable())
					{
						m_connected = true;
						m_worker = jthread([this](std::stop_token st) { RecvData(st); });
					}
					else
						returnVal = false;
				}
			}
		}
	}

	return returnVal;
}

bool TcpClient::CreateSocket(SOCKET_INFO socket)
{
	if (socket.socket != SOCKET_ERROR || socket.socket != INVALID_SOCKET)
	{
		m_clientSocket = socket;

		// 수신 스레드 동작 확인
		if (!m_worker.joinable())
		{
			m_connected = true;
			m_worker = jthread([this](std::stop_token st) { RecvData(st); });
		}
		else
			return false;
	}
	else
		return false;

	return true;
}


void TcpClient::Disconnect()
{
	m_connected = false;
	closesocket(m_clientSocket.socket);

	if (m_worker.joinable())
	{
		m_worker.request_stop();	// 스레드 종료 요청
		m_worker.join();
	}
}

void TcpClient::RecvData(std::stop_token tocken)
{
	int recvSize;
	int addrlen = sizeof(m_clientSocket.socketAddr);
	char data[65535];

	while (!tocken.stop_requested())
	{
		ZeroMemory(data, 1024);
		recvSize = recv(m_clientSocket.socket, data, 65535, 0);

		if (recvSize > 0)
		{
			m_pRecvBuf->Write((int8_t*)data, recvSize);
		}
	}
}

size_t TcpClient::GetData(int8_t* data, size_t rdSize)
{
	return m_pRecvBuf->Read(data, rdSize);
}


int TcpClient::SendPacket(void* pData, DWORD sendSize)
{
	if (m_connected)
		return send(m_clientSocket.socket, (char*)pData, sendSize, 0);
	else
		return -1;
}


