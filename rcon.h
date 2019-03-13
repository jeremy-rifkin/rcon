#include <string>
#include <exception>
#include <winsock2.h>
//#pragma comment(lib, "ws2_32.lib")
#include "types.h"


// This class is not, as it stands, multi-thread friendly.
class RCON {
	static const int SERVERDATA_AUTH = 3;
	static const int SERVERDATA_EXECCOMMAND = 2;
	static const int SERVERDATA_AUTH_RESPONSE = 2;
	static const int SERVERDATA_RESPONSE_VALUE = 0;
	SOCKET server;
	bool authenticated = false;
	bool connected = false;
	struct Packet {
		int32 size;
		int32 id;
		int32 type;
		char body[4087]; // Max value for packet size is 4096 bytes. 4096 - 4 - 4 - 1 == 4087; String should be null terminated
		char null = 0x0;
	} __attribute__((packed));
	static const int STD_TRANSMITION_ID = 0;
	static const int RESPONSE_END_DETECTOR_ID = 1;
	static const struct Packet RESPONSE_END_DETECTOR;
	struct linkedString {
		linkedString* next = 0x0;
		char* string;
		int length;
	};
public:
	RCON();
	RCON(std::string host, int port);
	RCON(char* host, int port);
	~RCON();
	void connect(std::string host, int port);
	void connect(char* host, int port);
	void auth(std::string password);
	void auth(char* password);
	char* executeCommand(const char* command);
	bool isAuthenticated();
	void disconnect();
	struct errorError: public std::exception {
		const char* what() const throw();
	};
	struct connectionError: public std::exception {
		char* errmsg = 0x0;
		connectionError(char* msg);
		const char* what() const throw();
	};
	struct authenticationError: public std::exception {
		const char* what() const throw();
	};
	struct protocolError: public std::exception {
		char* errmsg = 0x0;
		protocolError(char* msg);
		const char* what() const throw();
	};
	struct valueError: public std::exception {
		char* errmsg = 0x0;
		valueError(char* msg);
		const char* what() const throw();
	};
private:
	void initSocket(char* ip, int port);
	void SendPacket(const Packet* packet) const;
	void GetPacket(Packet* packet);
};
