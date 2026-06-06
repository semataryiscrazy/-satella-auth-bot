#include <string>
#include <vector>
#include <cstdlib>
#include <cstdio>
#include <cctype>
#include <chrono>
#include <algorithm>

#undef WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#define WIN32_LEAN_AND_MEAN
#include "../source/Cfg/strenc.h"
#include "ka_bridge.h"
#include <windows.h>
#include <wininet.h>
#include <winhttp.h>
#include <iphlpapi.h>
#define SECURITY_WIN32
#include <security.h>
#include <schannel.h>
#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "secur32.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "iphlpapi.lib")

static const char* KA_APP();
static const char* KA_OWNER();
static const char* KA_SECRET();
static const char* KA_VER();
static const char* KA_APP() { static const char* r = AY_OBFUSCATE("satella"); return r; }
static const char* KA_OWNER() { static const char* r = AY_OBFUSCATE("2muvrcPJ73"); return r; }
static const char* KA_SECRET() { static const char* r = AY_OBFUSCATE("ea66bf36a53fe812ae7713a9449cbfffebd4b65e48ca3f61bf6b9234e6737475"); return r; }
static const char* KA_VER() { static const char* r = AY_OBFUSCATE("1.0"); return r; }
static std::string g_user, g_err, g_sid;
static long long g_expiry = 0;

static std::string gw() {
    HW_PROFILE_INFOA i; return GetCurrentHwProfileA(&i) ? i.szHwProfileGuid : "";
}

static std::string enc(const std::string& s) {
    std::string o;
    for (unsigned char c : s) {
        if (isalnum(c)||c=='-'||c=='_'||c=='.'||c=='~') o+=c;
        else { char b[8]; sprintf_s(b,"%%%02X",c); o+=b; }
    }
    return o;
}

static std::string js_str(const std::string& j, const std::string& k) {
    std::string s="\""+k+"\":\""; auto p=j.find(s);
    if(p==j.npos){s="\""+k+"\": \"";p=j.find(s);}
    if(p==j.npos)return"";
    auto st=p+s.length(), en=j.find('"',st);
    return en==j.npos?"":j.substr(st,en-st);
}

static bool js_bool(const std::string& j, const std::string& k) {
    return j.find("\""+k+"\":true")!=j.npos||j.find("\""+k+"\": true")!=j.npos;
}

static bool send_all(SOCKET s,const char* d,int len);

static std::string dns_nslookup() {
    HANDLE rPipe,wPipe;
    SECURITY_ATTRIBUTES sa={sizeof(sa),NULL,TRUE};
    if(!CreatePipe(&rPipe,&wPipe,&sa,0))return"";
    SetHandleInformation(wPipe,HANDLE_FLAG_INHERIT,HANDLE_FLAG_INHERIT);
    STARTUPINFOA si={sizeof(si)}; si.dwFlags=STARTF_USESTDHANDLES; si.hStdOutput=wPipe; si.hStdError=wPipe;
    PROCESS_INFORMATION pi;
    char cmd[MAX_PATH]; GetSystemWindowsDirectoryA(cmd,sizeof(cmd));
    strcat_s(cmd,"\\system32\\nslookup.exe");
    char args[256]; sprintf_s(args,AY_OBFUSCATE("\"%s\" keyauth.win 8.8.8.8"),cmd);
    if(!CreateProcessA(cmd,args,NULL,NULL,TRUE,CREATE_NO_WINDOW,NULL,NULL,&si,&pi)){
        CloseHandle(rPipe); CloseHandle(wPipe); return"";
    }
    CloseHandle(pi.hThread); CloseHandle(wPipe);
    char buf[4096]; DWORD read=0;
    if(!ReadFile(rPipe,buf,sizeof(buf)-1,&read,NULL)||read<8){CloseHandle(rPipe);WaitForSingleObject(pi.hProcess,1000);CloseHandle(pi.hProcess);return"";}
    buf[read]=0; CloseHandle(rPipe);
    WaitForSingleObject(pi.hProcess,3000); CloseHandle(pi.hProcess);
    // extrai qualquer IPv4 do output
    for(const char* p=buf;*p;p++){
        if(isdigit((unsigned char)*p)){
            int a,b,c,d; int n=0;
            if(sscanf_s(p,"%d.%d.%d.%d%n",&a,&b,&c,&d,&n)==4&&n>0){
                if(a>=0&&a<=255&&b>=0&&b<=255&&c>=0&&c<=255&&d>=0&&d<=255){
                    char ip[64]; sprintf_s(ip,"%d.%d.%d.%d",a,b,c,d);
                    if(strcmp(ip,"0.0.0.0")!=0&&strcmp(ip,"8.8.8.8")!=0)return ip;
                }
            }
        }
    }
    return"";
}

static std::string try_ips() {
    const char* ips[]={AY_OBFUSCATE("104.26.0.5"),AY_OBFUSCATE("104.26.1.5"),AY_OBFUSCATE("172.67.72.57")};
    for(int i=0;i<3;i++){
        SOCKET s=socket(AF_INET,SOCK_STREAM,0);
        if(s==INVALID_SOCKET)continue;
        struct sockaddr_in sa={}; sa.sin_family=AF_INET; sa.sin_port=htons(443);
        inet_pton(AF_INET,ips[i],&sa.sin_addr);
        if(connect(s,(sockaddr*)&sa,sizeof(sa))==0){closesocket(s);return ips[i];}
        closesocket(s);
    }
    return"";
}

static std::string build_dns_q() {
    unsigned char q[512]; int slen;
    memset(q,0,512); q[0]=0x12;q[1]=0x34;q[2]=0x01;q[4]=0x01;
    unsigned char* p=q+12; int pos=0;
    const char* labels[]={AY_OBFUSCATE("keyauth"),AY_OBFUSCATE("win"),NULL};
    for(int i=0;labels[i];i++){int l=(int)strlen(labels[i]);p[pos++]=l;memcpy(p+pos,labels[i],l);pos+=l;}
    p[pos++]=0; p[pos++]=0x00; p[pos++]=0x01; p[pos++]=0x00; p[pos++]=0x01;
    slen=12+pos;
    std::string r; r.assign((char*)q,slen); return r;
}
static std::string parse_dns_q(const std::string& resp) {
    if(resp.size()<12||(unsigned char)resp[0]!=0x12||(unsigned char)resp[1]!=0x34||(resp[3]&0x0F)!=0)return"";
    int anc=((unsigned char)resp[6]<<8)|(unsigned char)resp[7]; if(anc<1)return"";
    int r=(int)resp.size(), off=12;
    while(off<r&&resp[off])off+=resp[off]+1; off+=5;
    if(off+12>r)return"";
    if(resp[off]&0xC0)off+=2;else{while(off<r&&resp[off])off+=resp[off]+1;off+=1;}
    off+=10; int rdlen=((unsigned char)resp[off]<<8)|(unsigned char)resp[off+1]; off+=2;
    if(rdlen!=4||off+4>r)return"";
    char ip[64]; sprintf_s(ip,"%d.%d.%d.%d",(unsigned char)resp[off],(unsigned char)resp[off+1],(unsigned char)resp[off+2],(unsigned char)resp[off+3]);
    return ip;
}
static std::string dns_udp(const std::string& svr) {
    SOCKET s=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
    if(s==INVALID_SOCKET)return"";
    struct sockaddr_in dns; dns.sin_family=AF_INET; dns.sin_port=htons(53);
    inet_pton(AF_INET,svr.c_str(),&dns.sin_addr);
    std::string q=build_dns_q();
    if(sendto(s,q.data(),(int)q.size(),0,(struct sockaddr*)&dns,sizeof(dns))==SOCKET_ERROR){closesocket(s);return"";}
    fd_set fds; FD_ZERO(&fds); FD_SET(s,&fds);
    struct timeval tv; tv.tv_sec=4; tv.tv_usec=0;
    if(select(0,&fds,NULL,NULL,&tv)<=0){closesocket(s);return"";}
    unsigned char resp[1024]; sockaddr_in from; int flen=sizeof(from);
    int r=recvfrom(s,(char*)resp,sizeof(resp),0,(struct sockaddr*)&from,&flen);
    closesocket(s);
    return parse_dns_q(std::string((char*)resp,r));
}
static std::string dns_tcp(const std::string& svr) {
    SOCKET s=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    if(s==INVALID_SOCKET)return"";
    struct sockaddr_in dns; dns.sin_family=AF_INET; dns.sin_port=htons(53);
    inet_pton(AF_INET,svr.c_str(),&dns.sin_addr);
    if(connect(s,(struct sockaddr*)&dns,sizeof(dns))!=0){closesocket(s);return"";}
    std::string q=build_dns_q();
    unsigned short nlen=htons((unsigned short)q.size());
    send_all(s,(char*)&nlen,2); send_all(s,q.data(),(int)q.size());
    if(recv(s,(char*)&nlen,2,0)!=2){closesocket(s);return"";}
    int rlen=ntohs(nlen); if(rlen<12||rlen>1024){closesocket(s);return"";}
    std::string resp; resp.resize(rlen); int rd=0;
    while(rd<rlen){int n=recv(s,&resp[rd],rlen-rd,0);if(n<=0)break;rd+=n;}
    closesocket(s); resp.resize(rd);
    return parse_dns_q(resp);
}

static std::string resolve_host() {
    WSADATA ws; WSAStartup(MAKEWORD(2,2),&ws); // Winsock stays init for whole process
    // 1) system DNS
    ADDRINFOA hints={}; hints.ai_family=AF_INET; hints.ai_socktype=SOCK_STREAM;
    ADDRINFOA *ai=NULL;
    if(getaddrinfo(AY_OBFUSCATE("keyauth.win"),AY_OBFUSCATE("443"),&hints,&ai)==0&&ai){
        struct sockaddr_in *sin=(struct sockaddr_in*)ai->ai_addr;
        char ip[64]; inet_ntop(AF_INET,&sin->sin_addr,ip,sizeof(ip));
        freeaddrinfo(ai); return ip;
    }
    if(ai)freeaddrinfo(ai);
    // 2) system DNS servers direct
    ULONG sz=0; GetAdaptersAddresses(AF_INET,GAA_FLAG_SKIP_ANYCAST|GAA_FLAG_SKIP_MULTICAST,NULL,NULL,&sz);
    PIP_ADAPTER_ADDRESSES aa=(PIP_ADAPTER_ADDRESSES)malloc(sz);
    if(aa&&GetAdaptersAddresses(AF_INET,GAA_FLAG_SKIP_ANYCAST|GAA_FLAG_SKIP_MULTICAST,NULL,aa,&sz)==ERROR_SUCCESS){
        for(PIP_ADAPTER_ADDRESSES a=aa;a;a=a->Next){
            for(PIP_ADAPTER_DNS_SERVER_ADDRESS d=a->FirstDnsServerAddress;d;d=d->Next){
                SOCKADDR_IN* sa=(SOCKADDR_IN*)d->Address.lpSockaddr;
                char svr[64]; inet_ntop(AF_INET,&sa->sin_addr,svr,sizeof(svr));
                std::string ip=dns_udp(svr); if(!ip.empty()){free(aa);return ip;}
            }
        }
    }
    free(aa);
    // 3) public DNS via UDP
    const char* udpSvrs[]={"8.8.8.8","1.1.1.1","8.8.4.4","9.9.9.9","208.67.222.222"};
    for(int i=0;i<5;i++){std::string ip=dns_udp(udpSvrs[i]);if(!ip.empty())return ip;}
    // 4) public DNS via TCP
    const char* tcpSvrs[]={"8.8.8.8","1.1.1.1","8.8.4.4","9.9.9.9","208.67.222.222"};
    for(int i=0;i<5;i++){std::string ip=dns_tcp(tcpSvrs[i]);if(!ip.empty())return ip;}
    // 5) nslookup
    {std::string ip=dns_nslookup();if(!ip.empty())return ip;}
    // 6) hardcoded IPs as last resort
    return try_ips();
}

static bool send_all(SOCKET s,const char* d,int len){
    while(len>0){int n=send(s,d,len,0);if(n<=0)return false;d+=n;len-=n;}return true;
}
static int recv_all(SOCKET s,char* b,int sz,int timeoutMs){
    if(timeoutMs>0){fd_set f;FD_ZERO(&f);FD_SET(s,&f);struct timeval tv;tv.tv_sec=timeoutMs/1000;tv.tv_usec=(timeoutMs%1000)*1000;
        if(select(0,&f,NULL,NULL,&tv)<=0)return -1;}
    return recv(s,b,sz,0);
}

static std::string post(const std::string& d) {
    std::string r;
    std::string ip=resolve_host();
    if(ip.empty()){g_err=AY_OBFUSCATE("DNS keyauth.win failed");printf(AY_OBFUSCATE("[ka] DNS resolve failed\n"));return r;}
    printf("[ka] Resolved IP: %s\n",ip.c_str());
    
    char tempDir[MAX_PATH];
    if(!GetTempPathA(MAX_PATH,tempDir)){g_err="GetTempPath failed";return r;}
    char outFile[MAX_PATH];
    if(!GetTempFileNameA(tempDir,"KO_",0,outFile)){g_err="GetTempFile failed";return r;}
    printf("[ka] Output file: %s\n",outFile);
    
    // ProgressPreference SilentlyContinue para nao gerar CLIXML
    std::string ps = std::string(AY_OBFUSCATE("$ProgressPreference='SilentlyContinue';"));
    ps+=std::string(AY_OBFUSCATE("$ip='")) + ip + std::string(AY_OBFUSCATE("';$body='")) + d + std::string(AY_OBFUSCATE("';"));
    ps+=std::string(AY_OBFUSCATE("$tcp=New-Object System.Net.Sockets.TcpClient($ip,443);"));
    ps+=std::string(AY_OBFUSCATE("$s=$tcp.GetStream();"));
    ps+=std::string(AY_OBFUSCATE("$ssl=New-Object System.Net.Security.SslStream($s,$false,{$true});"));
    ps+=std::string(AY_OBFUSCATE("$ssl.AuthenticateAsClient('keyauth.win',$null,[System.Security.Authentication.SslProtocols]::Tls12,$false);"));
    ps+=std::string(AY_OBFUSCATE("$ssl.ReadTimeout=15000;"));
    ps+=std::string(AY_OBFUSCATE("$req='POST /api/1.3/ HTTP/1.1'+[char]13+[char]10+'Host: keyauth.win'+[char]13+[char]10+'Content-Type: application/x-www-form-urlencoded'+[char]13+[char]10+'Content-Length: '")) + std::string(AY_OBFUSCATE("+$body.Length+[char]13+[char]10+'Connection: close'+[char]13+[char]10+[char]13+[char]10+$body;"));
    ps+=std::string(AY_OBFUSCATE("$bytes=[System.Text.Encoding]::UTF8.GetBytes($req);"));
    ps+=std::string(AY_OBFUSCATE("$ssl.Write($bytes,0,$bytes.Length);"));
    ps+=std::string(AY_OBFUSCATE("$out=[System.IO.File]::Create('")) + std::string(outFile) + std::string(AY_OBFUSCATE("');"));
    ps+=std::string(AY_OBFUSCATE("$buf=New-Object byte[] 4096;"));
    ps+=std::string(AY_OBFUSCATE("$ProgressPreference='SilentlyContinue';"));
    ps+=std::string(AY_OBFUSCATE("do{$n=$ssl.Read($buf,0,4096);if($n-gt0){$out.Write($buf,0,$n)}}while($n-gt0);"));
    ps+=std::string(AY_OBFUSCATE("$out.Close();$ssl.Close();$tcp.Close()"));
    printf("[ka] PS script len=%zu\n",ps.size());
    
    int wlen=MultiByteToWideChar(CP_UTF8,0,ps.c_str(),(int)ps.size(),NULL,0);
    wchar_t* wstr=(wchar_t*)malloc((wlen+1)*2);
    MultiByteToWideChar(CP_UTF8,0,ps.c_str(),(int)ps.size(),wstr,wlen);
    wstr[wlen]=0;
    DWORD b64len=0;
    CryptBinaryToStringA((BYTE*)wstr,wlen*2,CRYPT_STRING_BASE64,NULL,&b64len);
    char* b64=(char*)malloc(b64len);
    CryptBinaryToStringA((BYTE*)wstr,wlen*2,CRYPT_STRING_BASE64,b64,&b64len);
    free(wstr);
    std::string b64s(b64); free(b64);
    b64s.erase(std::remove(b64s.begin(),b64s.end(),'\r'),b64s.end());
    b64s.erase(std::remove(b64s.begin(),b64s.end(),'\n'),b64s.end());
    printf("[ka] Base64 cmd len=%zu\n",b64s.size());
    
    // Sem > redirect — stdout do PS vai pro nada (CREATE_NO_WINDOW)
    // O response HTTP vai pro outFile via File::Create
    std::string cmdLine=std::string(AY_OBFUSCATE("powershell.exe -ExecutionPolicy Bypass -EncodedCommand "))+b64s;
    printf("[ka] Cmd: %s\n",cmdLine.c_str());
    int cwlen=MultiByteToWideChar(CP_UTF8,0,cmdLine.c_str(),-1,NULL,0);
    wchar_t* cwstr=(wchar_t*)malloc(cwlen*2);
    MultiByteToWideChar(CP_UTF8,0,cmdLine.c_str(),-1,cwstr,cwlen);
    std::wstring cmdW(cwstr); free(cwstr);
    
    PROCESS_INFORMATION pi;
    HANDLE hNul=CreateFileA(AY_OBFUSCATE("NUL"),GENERIC_WRITE,FILE_SHARE_WRITE,NULL,OPEN_EXISTING,0,NULL);
    STARTUPINFOW si={sizeof(si)}; si.dwFlags=STARTF_USESTDHANDLES;
    si.hStdOutput=hNul; si.hStdError=hNul;
    if(!CreateProcessW(NULL,&cmdW[0],NULL,NULL,TRUE,CREATE_NO_WINDOW,NULL,NULL,&si,&pi)){
        CloseHandle(hNul);
        g_err="PowerShell launch failed"; printf("[ka] CreateProcess failed: %d\n",GetLastError());
        DeleteFileA(outFile); return r;
    }
    CloseHandle(hNul);
    printf("[ka] Waiting for PowerShell...\n");
    DWORD waitRet=WaitForSingleObject(pi.hProcess,30000);
    printf("[ka] PowerShell done (waitRet=%u)\n",waitRet);
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    
    // Read output file
    HANDLE hFile=CreateFileA(outFile,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
    if(hFile!=INVALID_HANDLE_VALUE){
        char buf[65536]; DWORD read;
        std::string result;
        while(ReadFile(hFile,buf,sizeof(buf),&read,NULL)&&read>0)
            result.append(buf,read);
        CloseHandle(hFile);
        printf("[ka] Read %zu bytes from output file\n",result.size());
        if(result.size()>200)printf("[ka] First 200: %.200s\n",result.c_str());
        else printf("[ka] Content: %s\n",result.c_str());
        
        auto hl=result.find("\r\n\r\n");
        if(hl!=std::string::npos){
            r=result.substr(hl+4);
            printf("[ka] HTTP body: %s\n",r.c_str());
        }
        else if(!result.empty()){
            r=result;
            if(result.find("HTTP/")==std::string::npos&&result.find("{")==std::string::npos){
                g_err="PS error: "+result.substr(0,400);
                printf("[ka] NOT HTTP\n");
            }else printf("[ka] No CRLFCRLF but looks HTTP/JSON\n");
        }
        else{g_err="Empty response";printf("[ka] Empty file\n");}
    }else{
        g_err="Failed to read output file";
        printf("[ka] Failed to open output file\n");
    }
    
    DeleteFileA(outFile);
    return r;
}

int ka_init() {
    g_err.clear();
    std::string d="type=init&ver="+std::string(KA_VER())+"&name="+std::string(KA_APP())+"&ownerid="+std::string(KA_OWNER())+"&secret="+std::string(KA_SECRET());
    std::string r=post(d);
    if(r.empty()){if(g_err.empty())g_err="Connection failed";return 0;}
    if(js_bool(r,"success")){g_sid=js_str(r,"sessionid");return 1;}
    if(g_err.empty())g_err=js_str(r,"message");
    if(g_err.empty())g_err="raw: "+r.substr(0,300);
    return 0;
}

int ka_login(const char* u, const char* p) {
    if(g_sid.empty()&&!ka_init())return 0;
    g_err.clear();
    std::string d="type=login&username="+enc(u)+"&pass="+enc(p)+"&code=&hwid="+gw()+"&sessionid="+g_sid+"&name="+std::string(KA_APP())+"&ownerid="+std::string(KA_OWNER());
    std::string r=post(d);
    if(r.empty()){if(g_err.empty())g_err="Connection failed";return 0;}
    if(js_bool(r,"success")){
        g_user=u;
        g_expiry=0;
        auto sp=r.find("\"subscriptions\":[");
        if(sp!=r.npos){
            auto ep=r.find("\"expiry\":\"",sp);
            if(ep!=r.npos){
                ep+=10; auto ee=r.find('"',ep);
                if(ee!=r.npos) g_expiry=std::stoll(r.substr(ep,ee-ep));
            }
        }
        return 1;
    }
    g_err=js_str(r,"message");if(g_err.empty())g_err="Login failed";
    return 0;
}

int ka_register(const char* u, const char* p, const char* k) {
    if(g_sid.empty()&&!ka_init())return 0;
    g_err.clear();
    std::string d="type=register&username="+enc(u)+"&pass="+enc(p)+"&key="+enc(k)+"&email=&hwid="+gw()+"&sessionid="+g_sid+"&name="+std::string(KA_APP())+"&ownerid="+std::string(KA_OWNER());
    std::string r=post(d);
    if(r.empty()){if(g_err.empty())g_err="Connection failed";return 0;}
    if(js_bool(r,"success")){g_user=u;return 1;}
    g_err=js_str(r,"message");if(g_err.empty())g_err="Register failed";
    return 0;
}

const char* ka_get_username(){return g_user.c_str();}
const char* ka_get_error(){return g_err.empty()?NULL:g_err.c_str();}
int ka_get_days_remaining(){
    if(g_expiry<=0)return 0;
    long long now=std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    long long diff=g_expiry-now;
    return diff>0?(int)(diff/86400):0;
}
void ka_cleanup(){g_sid.clear();g_user.clear();g_err.clear();g_expiry=0;}
