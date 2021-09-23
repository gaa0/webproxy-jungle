/*
 * adder.c - a minimal CGI program that adds two numbers together
 * serve_dynamic에서 Fork로 생성됨.
 */
/* $begin adder */
#include "csapp.h"

int main(void) {
  char *buf, *p, *method;
  char arg1[MAXLINE], arg2[MAXLINE], content[MAXLINE];
  int n1=0, n2=0;

  if ((buf = getenv("QUERY_STRING")) != NULL) {
    // strchr: 문자열 내에 일치하는 문자가 있는지 검사하는 함수 (리턴값: 해당 문자 위치)
    p = strchr(buf, '&');  // p = buf에서의 & 위치
    *p = '\0';  // &를 NULL로 바꿈 (strcpy에서 NULL 전까지 복사하기 때문)
    strcpy(arg1, buf);  // num1=1
    strcpy(arg2, p+1);  // num2=2

    // if (strchr(arg1, '=')) {  // 이 부분 없어도 되네????????? 크롬에서 돌리려면 있어야 함.
    //   p = strchr(arg1, '=');  // telnet에서는 바로 숫자만 넣으니까 되는 거
    //   *p = '\0';
    //   strcpy(arg1, p + 1);

    //   p = strchr(arg2, '=');
    //   *p = '\0';
    //   strcpy(arg2, p + 1);
    // }

    n1 = atoi(arg1);  // 문자 스트링을 정수로 변환
    n2 = atoi(arg2);
  }

  method = getenv("REQUEST_METHOD");

  // Make the response body
  // sprintf(content, "QUERY_STRING=%s", buf);  // 이건 왜 해주는 걸까
  sprintf(content, "Welcome to add.com: ");
  sprintf(content, "%sTHE Internet addition portal.\r\n<p>", content);
  sprintf(content, "%sThe answer is: %d + %d = %d\r\n<p>", content, n1, n2, n1 + n2);
  sprintf(content, "%sThanks for visiting!\r\n", content);

  // Generate the HTTP response
  printf("Connection: close\r\n");
  printf("Content-length: %d\r\n", (int)strlen(content));
  printf("Content-type: text/html\r\n\r\n");
  // 클라이언트에서 \r\n으로 빈 줄을 만들어주면 그 빈 줄을 기준으로 윗부분이 헤더,
  // 아랫부분이 바디가 됨.
  if (strcasecmp(method, "HEAD") != 0)  // 왜 *method == "HEAD"는 안 될까? *method = "H"
    printf("%s", content);
  fflush(stdout);  // 지정된 출력과 연관된 버퍼를 비움

  exit(0);
}
/* $end adder */
