/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);

int main(int argc, char **argv) {
  int listenfd, connfd;  // fd: 파일 또는 소켓을 지칭하기 위해 부여한 숫자
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  /* Check command line args */
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]);
  while (1) {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *) &clientaddr, &clientlen);  // line:netp:tiny:accept
    Getnameinfo((SA *) &clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    doit(connfd);   // line:netp:tiny:doit
    Close(connfd);  // line:netp:tiny:close
  }
}

// 한 개의 트랜잭션 처리
void doit(int fd) {
  int is_static;
  struct stat sbuf;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];
  rio_t rio;

  // Read request line and headers
  // &rio 주소를 가지는 읽기 버퍼를 만들고 초기화함. (rio_t 구조체 초기화)
  Rio_readinitb(&rio, fd);
  Rio_readlineb(&rio, buf, MAXLINE);  // 버퍼에서 읽은 것이 담겨 있음.
  // rio_readlineb 함수를 사용해서 요청 라인을 읽어들임.

  printf("Request headers:\n");  // 요청 라인을 읽고
  printf("%s", buf);  // "GET / HTTP/1.1"

  // 버퍼에서 자료형을 읽음. 요청 라인을 분석함.
  sscanf(buf, "%s %s %s", method, uri, version);
  
  // strcasecmp: 두 문자열의 길이와 내용이 같을 때 0 반환
  if (strcasecmp(method, "GET")) {
    clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");
    return;
  }
  read_requesthdrs(&rio);  // 요청 헤더를 읽는 함수

  // Parse URI from GET request
  // uri를 분석함. 파일이 없으면 에러를 띄움.
  // parse_uri를 들어가기 전에 filename과 cgiargs는 없음.
  // 이 URI를 CGI 인자 스트링으로 분석하고 요청이 정적 또는 동적 컨텐츠인지 flag 설정.
  is_static = parse_uri(uri, filename, cgiargs);
  
  // stat: 파일 정보를 불러오고 sbuf에 내용을 적어줌. ok: 0 / error: -1
  if (stat(filename, &sbuf) < 0) {
    clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file");
    return;
  }

  if (is_static) {  // Serve static content
  // S_ISREG: 일반 파일인가?
  // 파일 접근 권한 비트 S_IRUSR: 읽기 권한이 있나? / S_IXUSR: 실행 권한이 있나?
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");
      return;
    }
    serve_static(fd, filename, sbuf.st_size);
  } else {  // Serve dynamic content
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) {
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
      return;
    }
    serve_dynamic(fd, filename, cgiargs);
  }
}

void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg) {
  char buf[MAXLINE], body[MAXLINE];

  // Build the HTTP response body
  // sprintf: 출력하는 결과값을 변수에 저장
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Tiny web server</em>\r\n", body);

  // Print the HTTP response
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);

  // rio_writen: buf에서 fd로 strlen(buf)바이트 전송
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}

void read_requesthdrs(rio_t *rp) {  // 요청 헤더 왜 무시? -> 필요한 정보 있으면 무시x
  char buf[MAXLINE];

  Rio_readlineb(rp, buf, MAXLINE);  // 이 줄 없어도 되지 않나? 없어도 됨.
  while (strcmp(buf, "\r\n")) {
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
  }
  return;
}

int parse_uri(char *uri, char *filename, char *cgiargs) {
  char *ptr;

  // URI를 파일 이름과 옵션으로 CGI 인자 스트링을 분석함.
  // strstr: "cgi-bin"이 uri에 나타나지 않으면 NULL 리턴
  // cgi-bin이 없다면 (즉 정적 컨텐츠를 요청하면)
  if (!strstr(uri, "cgi-bin")) {  // Static content
    strcpy(cgiargs, "");  // CGI 인자 스트링을 지움.
    strcpy(filename, ".");

    // strcat: filname에 uri 연결 (스트링 연결 함수)
    strcat(filename, uri);

    // 상대 리눅스 파일 이름 ./home.html로 변환
    if (uri[strlen(uri) - 1] == '/')  // 만일 URI가 '/' 문자로 끝난다면
      strcat(filename, "home.html");  // 기본 파일 이름 추가
    return 1;
  } else {  // Dynamic content
    ptr = index(uri, '?');
    // 모든 CGI 인자들 추출
    if (ptr) {
      // 물음표 뒤에 있는 인자 다 갖다붙임.
      strcpy(cgiargs, ptr + 1);
      // 포인터는 문자열 마지막으로 바꿈.
      *ptr = '\0';
    } else
      strcpy(cgiargs, "");
    strcpy(filename, ".");  // 나머지 URI 부분을 상대 리눅스 파일 이름으로 변환
    strcat(filename, uri);  // ./uri
    return 0;
  }
}

// fd: 응답받는 소켓(연결식별자)
void serve_static(int fd, char *filename, int filesize) {
  int srcfd;
  char *srcp, filetype[MAXLINE], buf[MAXBUF];
  
  // Send response headers to client
  // 파일 이름의 접미어 부분을 검사해서 파일 타입 결정
  get_filetype(filename, filetype);

  // 클라이언트에 응답 줄과 응답 헤더를 보냄.
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
  sprintf(buf, "%sConnection: close\r\n", buf);
  sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);

  // 요청한 파일의 내용을 연결 식별자 fd로 복사해서 응답 본체 보냄.
  Rio_writen(fd, buf, strlen(buf));

  // 서버에 출력
  printf("Response headers:\n");
  printf("%s", buf);

  // Send response body to client
  // 읽기 위해서 filename을 오픈하고 식별자를 얻어옴.
  // O_RDONLY: 읽기 전용으로 열기
  // 0: 접근 권한
  srcfd = Open(filename, O_RDONLY, 0);

  // 리눅스 mmap 함수는 요청한 파일을 가상메모리 영역으로 매핑
  // mmap을 호출하면 파일 srcfd의 첫 번째 filesize 바이트 주소를 srcp에서 시작하는
  // 사적 읽기-허용 가상메모리 영역으로 매핑
  // mmap: 맵핑이 시작하는 실제 메모리 주소 리턴
  // 첫 번째 인자 *addr: 커널에게 파일을 어디에 맵핑하면 좋을지 제안하는 값
  // 두 번째 len: 맵핑시킬 메모리 영역의 길이
  // 세 번째 prot: 맵핑에 원하는 메모리 보호 정책
  // PROT_READ: 읽기 가능한 페이지 (읽기 전용)
  // 네 번째 flags: 맵핑 유형과 동작 구성 요소
  // MAP_PRIVATE: 맵핑을 공유하지 않아, 파일은 쓰기 후 복사로 맵핑되며,
  // 변경된 메모리 속성은 실제 파일에는 반영되지 않음.
  // 다섯 번째 fd: 파일 식별자
  // 여섯 번째 offset: 맵핑할 때 len의 시작점을 지정
  srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);

  // 파일을 메모리로 매핑한 후에 더 이상 이 식별자는 필요없어서 닫음.
  Close(srcfd);
  // 이렇게 하지 않으면 치명적인 메모리 누수 발생 가능

  // rio_writen 함수는 주소 srcp에서 시작하는 filesize 바이트(물론, 이것은 요청한
  // 파일에 매핑되어 있음)를 클라이언트의 연결 식별자로 복사
  Rio_writen(fd, srcp, filesize);

  // 매핑된 가상메모리 주소 반환. 이것은 치명적일 수 있는 메모리 누수를 피하는 데 중요.
  // munmap: mmap으로 만들어진 맵핑을 제거하기 위한 시스템 호출.
  Munmap(srcp, filesize);
}

// Derive file type from filename
void get_filetype(char *filename, char *filetype) {
  if (strstr(filename, ".html"))
    strcpy(filetype, "text/html");
  else if (strstr(filename, ".gif"))
    strcpy(filetype, "image/gif");
  else if (strstr(filename, ".png"))
    strcpy(filetype, "image/png");
  else if (strstr(filename, ".jpg"))
    strcpy(filetype, "image/jpeg");
  else if (strstr(filename, ".mp4"))
    strcpy(filetype, "video/mp4");
  else
    strcpy(filetype, "text/plain");
}

void serve_dynamic(int fd, char *filename, char *cgiargs) {
  char buf[MAXLINE], *empty_list[] = {NULL};

  // Return first part of HTTP response
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));
  
  // 새로운 자식 프로세스를 fork 함.
  // fork: 리눅스에서 제공하는 자식 프로세스를 생성하는 함수
  // fork에 의해 생성된 프로세스는 부모 프로세스의 복사본이므로, 원래 프로세스의 주소
  // 공간의 복사본으로 구성된다.
  if (Fork() == 0) {  // Child
    // Real server would set all CGI vars here
    // 자식은 QUERY_STRING 환경변수를 요청 URI의 CGI 인자들로 초기화함.
    // setenv 세 번째 인자 overwrite: 이미 같은 이름의 변수가 있다면 값을 변경할지
    setenv("QUERY_STRING", cgiargs, 1);

    // 자식은 자식의 표준 출력을 연결 파일 식별자로 재지정
    Dup2(fd, STDOUT_FILENO);  // Redirect stdout to client

    // CGI 프로그램을 로드하고 실행
    // execve: 실행가능한 파일인 filename의 실행코드를 현재 프로세스에 적재하여
    // 기존의 실행코드와 교체하여 새로운 기능으로 실행. 즉, 현재 실행되는 프로그램의
    // 기능은 없어지고 filename 프로그램을 메모리에 loading하여 처음부터 실행
    Execve(filename, empty_list, environ);  // Run CGI Program
    // CGI 프로그램이 자식 컨텍스트에서 실행되기 때문에 execve 함수를 호출하기 전에
    // 존재하던 열린 파일들과 환경변수들에도 동일하게 접근 가능. 그래서 CGI 프로그램이
    // 표준 출력에 쓰는 모든 것은 직접 클라이언트 프로세스로 부모 프로세스의 어떤
    // 간섭도 없이 전달됨.
  }
  // wait(NULL) : 자식 프로세스를 기다려라.
  // 부모는 자식이 종료되어 정리되는 것을 기다림.
  Wait(NULL);  // Parent waits for and reaps child
}