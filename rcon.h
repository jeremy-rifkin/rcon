#include <string>
#include <exception>

#ifdef _WIN32
typedef unsigned int SOCKET;
#elif __linux__
typedef int SOCKET;
#endif

// This class is not, as it stands, multi-thread friendly.
class RCON {
	// Constants
	static const int SERVERDATA_AUTH = 3;
	static const int SERVERDATA_EXECCOMMAND = 2;
	static const int SERVERDATA_AUTH_RESPONSE = 2;
	static const int SERVERDATA_RESPONSE_VALUE = 0;
	static const int STD_TRANSMITION_ID = 0;
	static const int RESPONSE_END_DETECTOR_ID = 1;
	// According to the spec, the max packet size is 4096. But it seems Valve can't implement their
	// own protocol correctly and srcds consistently sends packets with a size field slightly larger
	// than 4096 (on very long responses). I'll go with a max size of 4500 to accommodate stupidity.
	static const int MAX_PACKET_SIZE = 4500;
	static const int MAX_BODY_SIZE = MAX_PACKET_SIZE - 4 - 4 - 1;
	// Socket information
	SOCKET server;
	bool authenticated = false;
	bool connected = false;
	// Utility structures
	static struct Packet {
		int32_t size;
		int32_t id;
		int32_t type;
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
	static const int16_t __ed;
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
	
	static struct exception: public std::exception {
		exception() {};
		exception(char* msg, char* type);
		~exception();
		const char* what() const throw();
		const char* whatErr() const throw();
	private:
		char* errmsg;
		char* errtype;
	};
	struct socketError: public RCON::exception {
		socketError(char* msg): exception(msg, "Socket Error") {};
		socketError(char* msg, int ecode): exception(buildmsg(msg, ecode), "Socket Error") {};
	private:
		char* buildmsg(char* msg, int ecode);
	};
	struct authenticationError: public RCON::exception {
		authenticationError(): exception("Server rejected the authentication. Probably an "
											"incorrect password.", "Authentication Error") {};
	};
	struct protocolError: public RCON::exception {
		protocolError(char* msg): exception(msg, "Protocol Error") {};
	};
	struct valueError: public RCON::exception {
		valueError(char* msg): exception(msg, "Value Error") {};
	};
private:
	void initSocket(char* ip, int port);
	void sendPacket(const Packet* packet) const;
	void getPacket(Packet* packet);
	static inline void swapBytes_4(int32_t* n);
};
