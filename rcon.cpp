#include <cstring>
#ifdef _WIN32
#include <winsock2.h>
int GetLastSocketErrorCode() {
	return WSAGetLastError();
}
#elif __linux__
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
int GetLastSocketErrorCode() {
	return errno;
}
#endif
#include "rcon.h"

const int16_t RCON::__ed = 1;
const char* RCON::__edc = (char*) &RCON::__ed;
const bool RCON::IS_LITTLE_ENDIAN = RCON::__edc;
const struct RCON::Packet RCON::RESPONSE_END_DETECTOR = {4 + 4 + 1 + 1, RESPONSE_END_DETECTOR_ID, 0, 0x0, 0x0};

// (De-)Constructors
RCON::RCON() {

}
RCON::RCON(std::string host, int port) {
	connect(host.c_str(), port);
}
RCON::RCON(char* host, int port) {
	connect(host, port);
}
RCON::~RCON() {
	disconnect();
}

// Public methods
void RCON::connect(std::string host, int port) {
	connect(host.c_str(), port);
}
void RCON::connect(char* host, int port) {
	initSocket(host, port);
}
void RCON::auth(std::string password) {
	auth(password.c_str());
}
void RCON::auth(char* password) {
	if(password == 0x0)
		throw valueError("Password is null pointer.");
	// Authenticate with the server
	Packet authPacket;

	int passlen = 0;
	for(; (authPacket.body[passlen++] = password[passlen]) != 0x0 && passlen < 4087;); // Copy password into authPacket.body and count the password length // Note: password[passlen] is evaluated before authPacket.body[passlen++] // passlen will include the null terminator
	if(passlen == 4087 && password[passlen - 1] == 0x0)
		throw valueError("Password too long."); // Have to fail here. The protocol doesn't define multipacket auth/exec_command requests.
	authPacket.body[passlen] = 0x0;

	// Setup the rest of the packet
	authPacket.size = 4 + 4 + 1 + passlen;
	authPacket.id = STD_TRANSMITION_ID;
	authPacket.type = SERVERDATA_AUTH;

	sendPacket(&authPacket);

	Packet response;
	int tries = 0;
	// "When the server receives an auth request, it will respond with an empty SERVERDATA_RESPONSE_VALUE, followed immediately by a SERVERDATA_AUTH_RESPONSE indicating whether authentication succeeded or failed." - https://developer.valvesoftware.com/wiki/Source_RCON_Protocol
	// Handle SERVERDATA_RESPONSE_VALUE, which appears not to be sent in all implementations of this protocol
	while(tries++ < 2) {
		getPacket(&response);
		
		if(response.type == SERVERDATA_RESPONSE_VALUE && response.body[0] == 0x0) {
			continue; // Continue to SERVERDATA_AUTH_RESPONSE
		} else if(response.type == SERVERDATA_AUTH_RESPONSE) {
			if(response.id == -1) {
				throw authenticationError();
				return;
			}
			authenticated = true;
			break; 
		} else {
			throw protocolError("Unexpected packet while authenticating.");
		}
	}
	// Check that authentication did succede
	if(!authenticated) { // && !passwordError
		throw protocolError("Something weird happened while authenticating.");
	}
}
char* RCON::executeCommand(const char* command) {
	if(command == 0x0)
		throw valueError("Command is null pointer.");
	
	Packet cmdPacket;

	int cmdlen = 0;
	for(; (cmdPacket.body[cmdlen++] = command[cmdlen]) != 0x0 && cmdlen < 4087;); // cmdlen will include the null terminator
	if(cmdlen == 4087 && command[cmdlen - 1] == 0x0)
		throw valueError("Command too long."); // Have to fail here. The protocol doesn't define multipacket auth/exec_command requests.
	cmdPacket.body[cmdlen] = 0x0;

	// Setup the rest of the packet
	cmdPacket.size = 4 + 4 + 1 + cmdlen;
	cmdPacket.id = STD_TRANSMITION_ID;
	cmdPacket.type = SERVERDATA_EXECCOMMAND;

	sendPacket(&cmdPacket);
	sendPacket(&RESPONSE_END_DETECTOR);
	
	// Prepare linked string for response
	linkedString* responseHead = new linkedString;
	linkedString* currentNode = responseHead;
	int totalResponseLength = 0;

	// Get packets
	Packet responsePacket;
	while(true) {
		getPacket(&responsePacket);
		
		if(responsePacket.type != SERVERDATA_RESPONSE_VALUE) {
			throw protocolError("Unexpected packet while receiving response value.");
			continue;
		}
		if(responsePacket.id == RESPONSE_END_DETECTOR_ID) {
			// Source servers will mirror back the packet then send another packet pack with junk data
			// Minecraft servers will just send a packet complaining that it doesn't know what to do with the detector packet.
			// This will accommodate both
			if(responsePacket.size > RESPONSE_END_DETECTOR.size)
				break;
		}

		// Else handle packet
		// Copy string from packet
		int stringlen = responsePacket.size - 4 - 4 - 2;
		char* string = new char[stringlen];
		std::memcpy(string, responsePacket.body, stringlen);
		// Build on the linked string
		linkedString* nextNode = new linkedString;
		currentNode->next = nextNode;
		currentNode->string = string;
		currentNode->length = stringlen;
		currentNode = nextNode;
		totalResponseLength += stringlen;
	}
	
	// Construct final string
	char* response = new char[totalResponseLength + 1];
	int idx = 0;
	currentNode = responseHead;
	linkedString* lastNode;
	while(currentNode->next != 0x0) {
		// Copy string from link into response...
		std::memcpy(response + idx, currentNode->string, currentNode->length);
		idx += currentNode->length;
		// De-allocate link string
		delete[] currentNode->string;
		// Move onto the next node
		lastNode = currentNode;
		currentNode = currentNode->next;
		// Deallocate previous node
		delete lastNode;
	}
	response[totalResponseLength] = 0x0; // Insert null terminator

	return response;
}
bool RCON::isAuthenticated() {
	return authenticated;
}
void RCON::disconnect() {
	if(connected) {
		#ifdef _WIN32
		closesocket(server);
		WSACleanup();
		#elif __linux__
		close(server);
		#endif
		authenticated = false;
		connected = false;
	}
}

RCON::exception::exception(char* msg, char* type) {
	errmsg = msg;
	errtype = type;
}
RCON::exception::~exception() {
	delete[] errmsg;
	delete[] errtype;
}
const char* RCON::exception::what() const throw() {
	return errmsg;
}
const char* RCON::exception::whatErr() const throw() {
	return errtype;
}

char HEX[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
char* RCON::socketError::buildmsg(char* msg, int ecode) {
	// Final msg will be msg + " Error code: " + hex(ecode) + "."
	// max wsa ecode is 11031, which means the max hex length will be 4 digits
	// So the final msg will be at least 18 characters longer
	int l = 0; // l will include the null terminator
	for(; msg[l++] != 0x0;);
	char* fmsg = new char[l + 18];
	memcpy(fmsg, msg, l - 1);
	memcpy(fmsg + l - 1, " Error code: ", 13);
	char hex[4];
	int b = 0x1000;
	int n = ecode;
	for(int i = 0; i < 4; i++) {
		hex[i] = HEX[n / b];
		n %= b;
		b /= 16;
	}
	memcpy(fmsg + l + 13 - 1, hex, 4);
	fmsg[l + 17 - 1] = '.';
	fmsg[l + 17] = '\0';
	//delete[] msg;
	return fmsg;
}

void RCON::initSocket(char* ip, int port) {
	// Set up address
	sockaddr_in hostaddr;
	//memset(&hostaddr, '0', sizeof(serv_addr));
    hostaddr.sin_family = AF_INET;
    hostaddr.sin_port = htons(port);
    hostaddr.sin_addr.s_addr = inet_addr(ip);

	#ifdef _WIN32
	// Setup socket
	WSADATA wsa;
	if(WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
		throw socketError("Socket startup failed.", GetLastSocketErrorCode());
	}
	#elif __linux__
    
	#endif
	//Create socket
	if((server = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		throw socketError("Socket creation failed.", GetLastSocketErrorCode());
	}
	//Connect to server
	if(::connect(server, (struct sockaddr*)&hostaddr, sizeof(hostaddr)) < 0) {
		throw socketError("Socket connection failed.", GetLastSocketErrorCode());
	}
	// Update status
	connected = true;
	// Configure socket timeout
	int timeout = 5/*seconds*/ * 1000;
	if(setsockopt(server, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout)) == -1) {
		// Meh
	}
}
void RCON::sendPacket(const Packet* packet) const {
	char* buffer;
	int buffersize = packet->size + 4;
	if(IS_LITTLE_ENDIAN) {
		buffer = (char*)packet;
	} else {
		buffer = new char[buffersize];
		memcpy(buffer, packet, buffersize);
		// Convert from big endian to little endian
		swapBytes_4((int32_t*)(buffer));
		swapBytes_4((int32_t*)(buffer + 4));
		swapBytes_4((int32_t*)(buffer + 8));
	}
	int sent, i = 0;
	while(buffersize - i > 0) {
		sent = send(server, buffer + i, buffersize - i, 0);
		if(sent == -1) {
			throw socketError("Packet send failed.", GetLastSocketErrorCode());
		}
		i += sent;
	}
	if(!IS_LITTLE_ENDIAN) delete[] buffer;
}
void RCON::getPacket(Packet* packet) {
	char* packetbuffer = (char*)packet;
	int read;
	int totalread = 0;
	
	// Get packet header (12 bytes)
	while(totalread < 12) {
		read = recv(server, packetbuffer + totalread, 12 - totalread, 0);
		if(read == -1) {
			throw socketError("Packet recieve failed.", GetLastSocketErrorCode());
		} else if(read == 0) {
			throw protocolError("Got no data from socket, even though data was expected. This shouldn't happen.");
		}
		totalread += read;
		#ifdef _WIN32
		Sleep(10);
		#elif __linux__
		usleep(10000);
		#endif
	}
	
	if(!IS_LITTLE_ENDIAN) {
		// Convert little endian numbers values from the packet to big endian
		swapBytes_4((int32_t*)(packetbuffer));
		swapBytes_4((int32_t*)(packetbuffer + 4));
		swapBytes_4((int32_t*)(packetbuffer + 8));
	}

	int bodysize = packet->size - 4 - 4;
	if(bodysize < 1 || packet->size > MAX_PACKET_SIZE) {
		throw protocolError("Received improper or malformed header.");
	}

	// Get packet body (+ empty string)
	totalread = 0;
	while(totalread < bodysize) {
		read = recv(server, packetbuffer + 12 + totalread, bodysize - totalread, 0);
		if(read == -1) {
			throw socketError("Packet recieve failed", GetLastSocketErrorCode());
		} else if(read == 0) {
			throw protocolError("Got no data from socket, even though data was expected. This shouldn't happen.");
		}
		totalread += read;
		#ifdef _WIN32
		Sleep(10);
		#elif __linux__
		usleep(10000);
		#endif
	}
}

inline void RCON::swapBytes_4(int32_t* n) {
	*n = (*n & 0x000000ff)<<24 |
		 (*n & 0x0000ff00)<<8  |
		 (*n & 0x00ff0000)>>8  |
		 (*n & 0xff000000)>>24;
}
