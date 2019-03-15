#include <iostream>
#include <string>
#include <stdio.h>
#include <windows.h>
#include "rcon.h"
#include "dns.h"

const char* version = "1.1";

// Software pieces
void usage() {
	std::cout<<"rcon client version "<<version<<std::endl;
	std::cout<<"Usage:"<<std::endl;
	std::cout<<"rcon <host> [options]"<<std::endl;
	std::cout<<"<host> can be an ip or domain name and can include the port."<<std::endl;
	std::cout<<"Options:"<<std::endl;
	std::cout<<"    -h --help     Display help message."<<std::endl;
	std::cout<<"    -p <port>     RCON Port. Overrides port specified along with host, if present."<<std::endl;
	std::cout<<"    -P <password> RCON Password. If not present the user will be prompted for a password."<<std::endl;
	std::cout<<"    -c <command>  Command to execute. If present, the program will connect, authenticate, run the command, and then exit."<<std::endl;
	std::cout<<"    -s            Silent mode. Only relevant if -c is present. Silent mode will suppress everything except command response."<<std::endl;
	std::cout<<"    -S            Use a secure SCON protocol (will fail if the server does not support SCON). TBA."<<std::endl;
	std::cout<<"    -M            Support Minecraft colors and other formatting codes."<<std::endl;
}

RCON server;
bool silent = false;
bool mcColors = false;
bool commandMode = false;

bool ctrlc = false;
int WINAPI ctrlhandle(DWORD ctype) {
	ctrlc = true;
	server.disconnect();
	std::cout<<std::endl<<"Bye!"<<std::endl;
	exit(1);
}
char* mcColorLookup(char code) {
	switch(code) {
		case '0': // black
			return "\x1B[38;2;0;0;0m";
		case '1': // dark blue
			return "\x1B[38;2;0;0;170m";
		case '2': // dark green
			return "\x1B[38;2;0;170;0m";
		case '3': // dark aqua
			return "\x1B[38;2;0;170;170m";
		case '4': // dark red
			return "\x1B[38;2;170;0;0m";
		case '5': // dark purple
			return "\x1B[38;2;170;0;170m";
		case '6': // gold
			return "\x1B[38;2;255;170;0m";
		case '7': // gray
			return "\x1B[38;2;170;170;170m";
		case '8': // dark gray
			return "\x1B[38;2;85;85;85m";
		case '9': // blue
			return "\x1B[38;2;85;85;255m";
		case 'a': // green
			return "\x1B[38;2;85;255;85m";
		case 'b': // aqua
			return "\x1B[38;2;85;255;255m";
		case 'c': // red
			return "\x1B[38;2;255;85;85m";
		case 'd': // purple
			return "\x1B[38;2;255;85;255m";
		case 'e': // yellow
			return "\x1B[38;2;255;255;85m";
		case 'f': // white
			return "\x1B[38;2;255;255;255m";
		
		case 'k': // obf
			return "\x1B[5m";
		case 'l': // bold
			return "\x1B[1m";
		case 'm': // strikethrough
			return "\x1B[9m";
		case 'n': // underline
			return "\x1B[24m";
		case 'o': // italics
			return "\x1B[3m";
		case 'r': // reset
			return "\x1B[0m";
		default:
			return "";
	}
}
void executeCommand(const char* cmd, bool silent=false) {
	if(!commandMode || !silent) std::cout<<"\x1B[90mExecuting...\x1B[0m";
	char* resp;
	try {
		resp = server.executeCommand(cmd);
	} catch(std::exception& e) {
		std::cout<<"\r\x1B[90mExecution failed\x1B[0m";
		std::cout<<std::endl<<"\x1B[91mError\x1B[0m while executing command: "<<e.what()<<std::endl;
		return;
	}
	if(!commandMode || !silent) std::cout<<"\r            \r"; // Clear status message
	if(resp[0] != 0x0) { // If there was output...
		// Print response
		int i = 0,
			segmenti = 0;
		bool format;
		char* escape;
		for(; resp[i] != 0x0; i++) {
			format = false;
			if(mcColors) {
				//https://minecraft.gamepedia.com/Formatting_codes
				if(resp[i] == '\\' && resp[i + 1] == 'n') { // if seq == \n // lookahead is safe
					escape = "\n";
					format = true;
				} else if((unsigned char)resp[i] == 0xc2 && (unsigned char)resp[i + 1] == 0xa7) { // if seq == section sign, which in utf-8 is 0xC2 0xA7 // Lookahead is safe
					escape = mcColorLookup(resp[i + 2]); // Lookahead is still safe
					if(*escape != '\0')
						format = true;
				}
			}
			if(format) {
				resp[i] = '\0';
				if(i - 1 - segmenti > 0) {
					std::cout<<(resp + segmenti);
				}
				std::cout<<escape;
				i += 2;
				segmenti = i + 1;
			}
		}
		if(i - 1 - segmenti > 0) {
			std::cout<<(resp + segmenti);
		}
		// Reset colors
		if(mcColors) std::cout<<"\x1B[0m";
		// Output the newline character if necessary
		if(!(commandMode || resp[i - 1] == '\n'))
			std::cout<<"****"<<std::endl;
	}
	// Cleanup
	delete[] resp;
}

int main(int argc, char *argv[]) {
	// Perform initial setup
	if(!SetConsoleCtrlHandler(ctrlhandle, true)) {
		std::cout<<"Setup error "<<GetLastError()<<" while binding ctrl handle."<<std::endl;
		// Don't necessarily need to crit fail
	}
	// Make windows support ansi colors
	HANDLE hStdout;
	hStdout = GetStdHandle(STD_OUTPUT_HANDLE); 
	if(hStdout == INVALID_HANDLE_VALUE){
		std::cout<<"Setup error "<<GetLastError()<<" while acquiring stdout handle."<<std::endl;
		return 1;
	}
	DWORD mode = 0;
	if(!GetConsoleMode(hStdout, &mode)){
		std::cout<<"Setup error "<<GetLastError()<<" while acquiring console mode info. Continuing."<<std::endl;
		// Don't necessarily need to crit fail
	}
	if(!SetConsoleMode(hStdout, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING)){
		std::cout<<"Setup error "<<GetLastError()<<" while setting console mode. Continuing."<<std::endl;
		// Don't necessarily need to crit fail
	}

	// Program info
	// Host info
	char* host = 0x0;
	char* port_c = 0x0;
	int port = 0x0;
	// Auth info
	char* password = 0x0;
	// Other parameters
	char* command = 0x0;

	// Begin parsing command line arguments
	if(argc == 1 || strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
		usage();
		return 0;
	}
	
	host = argv[1];
	// Handle if host has a ":"
	for(int i = 0; host[i] != 0x0; i++) {
		if(host[i] == ':') {
			host[i] = 0x0; // Null terminate host in the place of the ":"
			port_c = &host[i + 1]; // ... and point port to what comes immediately after // Lookahead is safe
			break;
		}
	}

	// Parse command line arguments
	for(int i = 2; i < argc; i++) {
		if(argv[i][0] == '-') { // then parse the option
			// Also facilitate the support of multiple flags at once... e.g. -abc or -abc aOPT cOPT
			int la = 1;
			for(int j = 1; argv[i][j] != 0x0; j++) {
				switch(argv[i][j]) {
					case 'p':
						if(i + la >= argc) {
							std::cout<<"\x1B[91mError\x1B[0m: Expected parameter after option -"<<argv[i][j]<<std::endl;
						}
						port_c = argv[i + la++];
						break;
					case 'P':
						if(i + la >= argc) {
							std::cout<<"\x1B[91mError\x1B[0m: Expected parameter after option -"<<argv[i][j]<<std::endl;
						}
						password = argv[i + la++];
						break;
					case 'c':
						if(i + la >= argc) {
							std::cout<<"\x1B[91mError\x1B[0m: Expected parameter after option -"<<argv[i][j]<<std::endl;
						}
						commandMode = true;
						command = argv[i + la++];
						break;
					case 's':
						silent = true;
						break;
					case 'M':
						mcColors = true;
						break;
					case 'S':
						std::cout<<"\x1B[91mError\x1B[0m: Secure protocol not implemented yet."<<std::endl;
						return 1;
						break;
					default:
						std::cout<<"\x1B[91mError\x1B[0m: Unknown option \""<<argv[i][j]<<"\""<<std::endl;
						break;
				}
			}
			i += la - 1; // Skip over any parameter parameters
		} else {
			std::cout<<"\x1B[91mError\x1B[0m: Unexpected command line argument \""<<argv[i]<<"\""<<std::endl;
			//return 1;
		}
	}

	// Begin verifying that parameters are valid
	// Check the port
	if(port_c == 0x0) {
		std::cout<<"\x1B[91mError\x1B[0m: Must specify port"<<std::endl;
		return 1;
	}
	int portlen = 0;
	if(port_c[0] == '-') {
		std::cout<<"\x1B[91mError\x1B[0m: Port is negative"<<std::endl;
		return 1;
	}
	// Check that port is numeric
	for(int i = 0; ; i++) {
		if(port_c[i] == 0x0)
			break;
		portlen++;
		if(port_c[i] < '0' || port_c[i] > '9') { // if port_c[i] is not a number...
			std::cout<<"\x1B[91mError\x1B[0m: Port is non-numeric"<<std::endl;
			return 1;
		}
	}
	// Convert port_c to int
	port = 0;
	int mul = 1;
	if(portlen > 5) { // This check prevents an excessively long port, which could cause mul to overflow
		std::cout<<"\x1B[91mError\x1B[0m: Port is too large! Maximum port is 65535."<<std::endl;
		return 1;
	}
	for(int i = portlen - 1; i >= 0; i--) {
		port += (port_c[i] - '0') * mul;
		mul *= 10;
	}
	// Perform final check on port
	if(port > 65535) {
		std::cout<<"\x1B[91mError\x1B[0m: Port is too large! Maximum port is 65535."<<std::endl;
		return 1;
	}

	// Begin checking host
	// Check form of host
	// Essentially regex...
	bool hostIsIPv4 = true;
	int i = 0;
	int b, n;
	for(b = 0; ; b++) {
		for(n = 0; ; n++) {
			if(host[i] < '0' || host[i] > '9') // if host[i] is not a number...
				break;
			i++;
		}
		if(n > 3) { // too many numbers
			hostIsIPv4 = false;
			break;
		}
		if(host[i] == 0x0) break; // end of string
		if(host[i] != '.') {
			hostIsIPv4 = false;
			break;
		}
		i++;
	}
	if(b != 3)
		hostIsIPv4 = false;
	// Perform host lookup if necessary
	char* hostIP;
	if(hostIsIPv4)
		hostIP = host;
	else {
		int status = DNS_Lookup(host, hostIP);
		if(status) {
			std::cout<<"\x1B[91mError\x1B[0m: DNS lookup failed with error code "<<std::hex<<std::uppercase<<status<<std::endl;
			std::cout<<"Make sure the hostname is just a domain name (e.g. no slashes, \"http://\", etc.)"<<std::endl;
			return 1;
		}
	}
	if(!commandMode || !silent) std::cout<<"Connecting to "<<hostIP<<" on port "<<port<<std::endl;
	
	server.connect(hostIP, port);
	
	
	// Begin authentication
	if(password == 0x0) { // Then user needs to enter password
		std::cout<<"Password: ";
		//https://stackoverflow.com/questions/6899025/hide-user-input-on-password-prompt
		HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE); 
		DWORD mode = 0;
		GetConsoleMode(hStdin, &mode);
		SetConsoleMode(hStdin, mode & (~ENABLE_ECHO_INPUT));
		std::string s;
		getline(std::cin, s);
		char* s_ = new char[s.size() + 1];
		strcpy(s_, s.c_str());
		password = s_;
		SetConsoleMode(hStdin, mode | ENABLE_ECHO_INPUT);
	}

	if(!commandMode || !silent) std::cout<<"\x1B[90mAuthenticating...\x1B[0m";
	try {
		server.auth(password);
	} catch(std::exception& e) {
		std::cout<<std::endl<<"\x1B[91mError\x1B[0m while authenticating: "<<e.what()<<std::endl;
		return 1;
	}
	delete[] password; // For safe measure (not that rcon is a secure protocol to begin with...)
	if(!commandMode || !silent) std::cout<<"\r\x1B[90mAuthentication success\x1B[0m"<<std::endl;

	// Handle command mode
	if(commandMode) {
		if(!silent) std::cout<<host<<"> "<<command<<std::endl;
		if(command[0] != 0x0)
			executeCommand(command, silent);
		server.disconnect();
		return 0;
	}

	// Begin console
	std::string command_;
	while(true) {
		if(ctrlc) {
			Sleep(5 * 1000);
			break;
		}
		std::cout<<host<<"> ";
		getline(std::cin, command_);
		if(command_ == "")
			continue;
		executeCommand(command_.c_str());
	}
	return 0;
}

// TODO: Linux
// TODO: Test DNS Resolution
// TODO: More color support?
// TODO: Support ipv6?
// TODO: Improve formatting and printing system
