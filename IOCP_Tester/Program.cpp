#include <iostream>
#include <regex>
#include <string>
#include <stop_token>
#include <thread>
#include "Comm/CircularBuf.h"
#include "Comm/TcpClient.h"

// 패킷
#pragma pack(push, 1)
struct DataPacket
{
	// header
	uint16_t id;
	uint16_t len;
};
#pragma pack(pop)

#define PACKET_SIZE 10 * 1024
#define BUF_SIZE 100 * 1024


std::jthread worker;
std::unique_ptr<CircularBuf> procBuf;
std::unique_ptr<TcpClient> tcpClient;

void DataProc()
{
	DataPacket packet;
	char payload[PACKET_SIZE];
	while (procBuf->DataSize() > 0)
	{
		// 패킷 크기 못채움
		if (procBuf->DataSize() < sizeof(DataPacket))
			break;

		procBuf->Peek((int8_t*)&packet, 4);

		if (packet.len > procBuf->DataSize())
		{
			break;
		}

		procBuf->Read((int8_t*)&packet, sizeof(DataPacket));
		procBuf->Read((int8_t*)payload, packet.len - sizeof(DataPacket));

		printf("%d byte 수신\n", packet.len);
	}
}


void GetRecvData(std::stop_token token)
{
	// 임시 저장 공간
	int8_t* tempSpace = new int8_t[BUF_SIZE];
	int getSize = 0;
	while (!token.stop_requested())
	{
		getSize = tcpClient->GetData(tempSpace, BUF_SIZE);
		if (getSize > 0)
		{
			procBuf->Write(tempSpace, getSize);
			DataProc();
		}
		else
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
}

bool IsValidIp(const std::string& ip)
{
	const std::regex pattern(R"(^((25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.){3}(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$)");
	return std::regex_match(ip, pattern);
}

int main()
{
	tcpClient = make_unique<TcpClient>(BUF_SIZE);
	procBuf = make_unique<CircularBuf>(BUF_SIZE);

	while (true)
	{
		std::cout << "연결 서버 IP: ";
		string ip;
		std::cin >> ip;
		if (!IsValidIp(ip))
		{
			std::cout << "유효하지 않은 IP 입력\n";
			continue;
		}

		std::cout << "연결 Port: ";
		int port;
		std::cin >> port;

		if (port < 0 || port > 65535)
		{
			std::cout << "0 ~ 65535 포트 입력\n";
			continue;
		}

		DataPacket testData{};
		char* packet;
		uint32_t size = 0;
		while (true)
		{
			std::cout << "100ms Test byte 크기 (0 ~ 8096): ";
			std::cin >> size;

			if (size > 8096)
				std::cout << "입력값 오류\n";
			else
			{
				testData.len = size + 4;		// data + header
				packet = new char[testData.len];
				ZeroMemory(packet, testData.len);

				memcpy(packet, &testData, 4);

				break;
			}
		}

		if (tcpClient->CreateSocket(ip, port))
		{
			std::string temp;
			std::cout << "전송 시작 (아무키 입력): ";
			std::cin >> temp;

			worker = std::jthread([](std::stop_token token) { GetRecvData(token); });

			while (true)
			{
				auto sSize = tcpClient->SendPacket(packet, testData.len);
				if (sSize > 0)
					printf("%d byte 송신\n", sSize);
				else
					printf("send error %d\n", WSAGetLastError());

				Sleep(100);
			}
		}
		else
		{
			std::cout << "서버 연결 실패 (Code: " << WSAGetLastError() << ")" << std::endl;
		}
	}
}

