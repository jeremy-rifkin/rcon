#include <winsock2.h>
#include <windns.h>

int DNS_Lookup(char* host, char*& ret) {
	DNS_STATUS status;
    PDNS_RECORD pDnsRecord;
	WORD wType = DNS_TYPE_A;
    char* pOwnerName = host;
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
