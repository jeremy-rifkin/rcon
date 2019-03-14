#include <string>
#include <exception>
#include <winsock2.h>
//#pragma comment(lib, "ws2_32.lib")
#include "types.h"


// This class is not, as it stands, multi-thread friendly.
class RCON {
	// Constants
	static const int SERVERDATA_AUTH = 3;
	static const int SERVERDATA_EXECCOMMAND = 2;
	static const int SERVERDATA_AUTH_RESPONSE = 2;
	static const int SERVERDATA_RESPONSE_VALUE = 0;
	static const int STD_TRANSMITION_ID = 0;
	static const int RESPONSE_END_DETECTOR_ID = 1;
	static const int MAX_PACKET_SIZE = 4500; // According to the spec the max packet size is 4096 but Valve can't implement their own fucking protocol and srcds consistently sends packets with a size field slightly larger than 4096 (on very long responses).
	static const int MAX_BODY_SIZE = MAX_PACKET_SIZE - 4 - 4 - 1;
	// Socket information
	SOCKET server;
	bool authenticated = false;
	bool connected = false;
	// Utility structures
	struct Packet {
		int32 size;
		int32 id;
		int32 type;
		char body[MAX_BODY_SIZE];
		char null = 0x0;
	} __attribute__((packed));
	struct linkedString {
		linkedString* next = 0x0;
		char* string;
		int length;
	};
	static const struct Packet RESPONSE_END_DETECTOR;
	// Endianness detection
	static const int16 __ed;
	static const char* __edc;
	static const bool IS_LITTLE_ENDIAN;
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
	void sendPacket(const Packet* packet) const;
	void getPacket(Packet* packet);
	static inline void swapBytes_4(int32* n);
};
