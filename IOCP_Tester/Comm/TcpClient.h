
#include "CircularBuf.h"
#include "CommHeader.h"

#if 0
#ifdef CREATEDLL_EXPORTS
#define DLL_CLASS __declspec(dllexport)
#else
#define DLL_CLASS __declspec(dllimport)
#endif
#endif

using namespace std;

class TcpClient
{
private:
	SOCKET_INFO m_clientSocket;
	jthread m_worker;				// 수신 스레드

	bool m_connected;

public:
	unique_ptr<CircularBuf> m_pRecvBuf;

public:
	TcpClient(int bufSize = 8 * 1024);
	~TcpClient();

	void StopRecv();

	bool CreateSocket(string server_ip, int serverPort, int localPort = 0);
	bool CreateSocket(SOCKET_INFO socket);
	void Disconnect();

	bool IsConnected() { return m_connected; }		// 연결 상태
	SOCKADDR_IN GetConnectInfo() { return m_clientSocket.socketAddr; }		// 클라이언트 정보 (서버용)

	void RecvData(std::stop_token tocken);
	size_t GetData(int8_t* data, size_t rdSize);

	int SendPacket(void* pData, DWORD sendSize);
};

