#ifdef _WIN32
#include <winsock2.h>
#include <windns.h>

int DNS_Lookup(char* host, char*& ret) {
	DNS_STATUS status;
    PDNS_RECORD pDnsRecord;
	WORD wType = DNS_TYPE_A;
    PCSTR pOwnerName = (PCSTR)host;
    DNS_FREE_TYPE freetype = DnsFreeRecordList;
    IN_ADDR ipaddr;

	status = DnsQuery(pOwnerName, wType, DNS_QUERY_BYPASS_CACHE, 0x0, &pDnsRecord, 0x0);
	
	if(status)
		return status;
	else {
		ipaddr.S_un.S_addr = (pDnsRecord->Data.A.IpAddress);
		ret = inet_ntoa(ipaddr);
		// Free memory allocated for DNS records.
		DnsRecordListFree(pDnsRecord, freetype);
	}
	return 0;
}

#elif __linux__
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <errno.h>

int DNS_Lookup(char* host, char*& ret) {
	struct hostent* _host;
	if((_host = gethostbyname(host)) == 0) {
		return errno;
	}
	struct in_addr** addr_list = (struct in_addr**) _host->h_addr_list;
	for(int i = 0; addr_list[i] != 0; i++) {
		ret = inet_ntoa(*addr_list[i]);
		return 0;
	}
	return -1;
}
#endif
