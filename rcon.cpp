#include <cstring>
#include "rcon.h"

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
		throw protocolError("Password too long."); // TODO
	if(passlen < 4087)
		authPacket.body[passlen] = 0x0;

	// Setup the rest of the packet
	authPacket.size = 4 + 4 + 1 + passlen;
	authPacket.id = STD_TRANSMITION_ID;
	authPacket.type = SERVERDATA_AUTH;

	SendPacket(&authPacket);

	Packet response; // TODO Just reuse authPacket?
	int tries = 0;
	// "When the server receives an auth request, it will respond with an empty SERVERDATA_RESPONSE_VALUE, followed immediately by a SERVERDATA_AUTH_RESPONSE indicating whether authentication succeeded or failed." - https://developer.valvesoftware.com/wiki/Source_RCON_Protocol
	// Handle SERVERDATA_RESPONSE_VALUE, which appears not to be sent in all implementations of this protocol
	while(tries++ < 2) {
		GetPacket(&response);
		
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
		throw protocolError("Command too long."); // TODO
	if(cmdlen < 4087)
		cmdPacket.body[cmdlen] = 0x0;

	// Setup the rest of the packet
	cmdPacket.size = 4 + 4 + 1 + cmdlen;
	cmdPacket.id = STD_TRANSMITION_ID;
	cmdPacket.type = SERVERDATA_EXECCOMMAND;

	SendPacket(&cmdPacket);
	SendPacket(&RESPONSE_END_DETECTOR);
	
	// Prepare linked string for response
	linkedString responseHead;
	linkedString* currentNode = &responseHead;
	int totalResponseLength = 0;

	// Get packets
	Packet responsePacket;
	while(true) {
		GetPacket(&responsePacket);
		if(responsePacket.type != SERVERDATA_RESPONSE_VALUE) {
			throw protocolError("Unexpected packet while receiving response value.");
			continue;
		}
		if(responsePacket.id == RESPONSE_END_DETECTOR_ID) {
			break; // Received mirror of the blank packet
		}
		// Else handle packet
		// Copy string from packet
		int stringlen = responsePacket.size - 4 - 4 - 1;
		char* string = new char[stringlen - 1];
		std::memcpy(string, responsePacket.body, stringlen - 1);
		// Build on the linked string
		linkedString nextNode;
		currentNode->next = &nextNode;
		currentNode->string = string;
		currentNode->length = stringlen - 1;
		currentNode = &nextNode;
		totalResponseLength += stringlen - 1;
	}
	
	// Construct final string
	char* response = new char[totalResponseLength + 1];
	int idx = 0;
	currentNode = &responseHead;
	while(currentNode->next != 0x0) {
		// Copy string from link into response...
		std::memcpy(response + idx, currentNode->string, currentNode->length);
		idx += currentNode->length;
		// De-allocate link string
		delete[] currentNode->string;
		// Move onto the next node
		currentNode = currentNode->next;
	}
	response[totalResponseLength] = 0x0; // Insert null terminator

	return response;
}
bool RCON::isAuthenticated() {
	return authenticated;
}
void RCON::disconnect() {
	if(connected) {
		closesocket(server);
		WSACleanup();
		authenticated = false;
		connected = false;
	}
}

const char* RCON::errorError::what() const throw() {
	return "The error wasn't initialized properly.";
}
RCON::connectionError::connectionError(char* msg) {
	errmsg = msg;
}
const char* RCON::connectionError::what() const throw() {
	if(errmsg == 0x0)
		throw errorError();
	return errmsg;
}
const char* RCON::authenticationError::what() const throw() {
	return "Server rejected the authentication. Probably an incorrect password.";
}
RCON::protocolError::protocolError(char* msg) {
	errmsg = msg;
}
const char* RCON::protocolError::what() const throw() {
	if(errmsg == 0x0)
		throw errorError();
	return errmsg;
}
RCON::valueError::valueError(char* msg) {
	errmsg = msg;
}
const char* RCON::valueError::what() const throw() {
	if(errmsg == 0x0)
		throw errorError();
	return errmsg;
}

void RCON::initSocket(char* ip, int port) {
	// Setup socket
	WSADATA wsa;
	struct sockaddr_in hostaddr;

	if(WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
		throw connectionError("Socket startup failed."); // TODO: also include WSAGetLastError()?
		return;
	}
	//Create a socket
	if((server = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
		throw connectionError("Socket creation failed."); // TODO: also include WSAGetLastError()?
	}
	
	hostaddr.sin_addr.s_addr = inet_addr(ip);
	hostaddr.sin_family = AF_INET;
	hostaddr.sin_port = htons(port);

	//Connect to remote server
	if(::connect(server, (struct sockaddr *)&hostaddr, sizeof(hostaddr)) < 0) {
		throw connectionError("Socket connection failed."); // TODO: also include WSAGetLastError()?
	}

	connected = true;

	DWORD timeout = 5/*seconds*/ * 1000;
	setsockopt(server, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
}
void RCON::SendPacket(const Packet* packet) const {
	if(send(server, (char*)packet, packet->size + 4, 0) < 0) {
		throw connectionError("Packet send failed");
	}
}
void RCON::GetPacket(Packet* packet) {
	int read = 0;

	if(!packetsInBuffer) {
		read = recv(server, tcpBuffer + bufferOffset, maxPacketSize - bufferOffset, 0);
		
		if(read == SOCKET_ERROR) {
			throw connectionError("Packet recieve failed");
		} else if(read == 0) {
			throw protocolError("Got no data from socket, even though data was expected. This shouldn't happen.");
		}
	}

	// Extract packet
	// Assumes start of packet is aligned with start of buffer
	// That should be a safe assumption, unless packets aren't being sent correctly
	int i;
	// The body starts 12 bytes into the packet
	for(i = 12; tcpBuffer[i] != 0x0 && i < bufferOffset + read; i++);
	if(tcpBuffer[i] != 0x0)
		throw protocolError("Some weird error happened, most likely a malformed packet.");
	// Check for the double null terminator
	if(!(i + 1 < bufferOffset + read && tcpBuffer[i + 1] == 0x0))
		throw protocolError("Double null terminator not found.");
	i++;

	// Copy packet from the buffer
	memcpy(packet, tcpBuffer, i);
	i++;

	// Move remaining tcp stream data to the front of the buffer
	memcpy(tcpBuffer, tcpBuffer + i, read - i);
	bufferOffset = read - i;

	if(packetsInBuffer)
		packetsInBuffer = false;

	// Check for other complete packets in the buffer
	for(i = 12; tcpBuffer[i] != 0x0 && i < bufferOffset - 1; i++);
	if(tcpBuffer[i] != 0x0)
		return;
	if(!(i + 1 < bufferOffset - 1 && tcpBuffer[i + 1] == 0x0))
		return;
	packetsInBuffer = true;
}
