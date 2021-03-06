#include "minet_socket.h"
#include <stdlib.h>
#include <ctype.h>
#include <string> //will be used to process string
#define BUFSIZE 1024

using std::string;

int write_n_bytes(int fd, char * buf, int count);
char *build_request_line(char *host, char *page);
const char *USERAGENT = "Mozilla/5.0";

int main(int argc, char * argv[]) {
    char * server_name = NULL;
    int server_port = 0;
    char * server_path = NULL;

    int sock = 0;
    int rc = -1;
    int datalen = 0;
    bool ok = true;
    struct sockaddr_in sa;
    FILE * wheretoprint = stdout;
    struct hostent * site = NULL;
    char * req = NULL;

    char buf[BUFSIZE + 1];
    //char * bptr = NULL;
    //char * bptr2 = NULL;
    char * endheaders = NULL;
   
    struct timeval timeout;
    fd_set set;

    /*parse args */
    if (argc != 5) {
	fprintf(stderr, "usage: http_client k|u server port path\n");
	exit(-1);
    }

    server_name = argv[2];
    server_port = atoi(argv[3]);
    server_path = argv[4];

    /* initialize minet */
    if (toupper(*(argv[1])) == 'K') { 
	minet_init(MINET_KERNEL);
    } else if (toupper(*(argv[1])) == 'U') { 
	minet_init(MINET_USER);
    } else {
	fprintf(stderr, "First argument must be k or u\n");
	exit(-1);
    }

    /* create socket */
    sock = minet_socket(SOCK_STREAM);//sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        fprintf(stderr, "Failed to create minet socket!\n");
        exit(-1);
    }
    // Do DNS lookup
    site = gethostbyname(server_name);
    if (site == NULL) {
	fprintf(stderr, "Failed to resolve hostname!\n");
	minet_close(sock);//close(sock);
	exit(-1);//return -1;
    }
    /* set address */
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(server_port); //the reverse byte stuff
    sa.sin_addr.s_addr = * (unsigned long *)site->h_addr_list[0];
    /* connect socket */
    if (minet_connect(sock, (struct sockaddr_in *) &sa) != 0) {
	fprintf(stderr, "Failed to connect to socket!\n");
        minet_close(sock);
        exit(-1);
    } 
    /*if (connect(sock, (struct sockaddr *)&sa, sizeof(sa)) != 0) {
	close(sock);
	return -1;}*/
    /* send request */
    req = build_request_line(server_name,server_path);
    datalen = strlen(req)+1;
    //rc = write_n_bytes(sock, req, datalen);
    //printf("build up request: %d", rc);
    rc = minet_write(sock, req, datalen);
    if(rc < 0) {
	fprintf(stderr, "Failed to send request!\n");
	minet_close(sock);
	exit(-1);
	} 
    /* wait till socket can be read */
    //FD_CLR(sock, &set);  //remove fd from the set
    //FD_ISSET(sock, &set);
    FD_ZERO(&set); //clears a file descriptor set
    FD_SET(sock, &set); //adds fd to the set
    if(FD_ISSET(sock, &set) == 0) {
       fprintf(stderr, "Failed to add sock to the set!\n");
       minet_close(sock);
       exit(-1);
    }
    minet_select(sock+1, &set, 0, 0, &timeout); //numfds, readfds, writefds, exception, timeout
    //select(1, &set, NULL, NULL, &timeout);
    /* Hint: use select(), and ignore timeout for now. */
    
    /* first read loop -- read headers */
    memset(buf, 0, sizeof(buf));
    string response = "";
    string header_text = "";
    endheaders = "\r\n\r\n";
    string::size_type pos;
    while((rc = minet_read(sock, buf, BUFSIZE)) > 0) {
	buf[rc] = '\0';
	response = response + (string)buf;
	if((pos= response.find(endheaders,0)) != string::npos) {
	   header_text = response.substr(0,pos);
	   response = response.substr(pos + 4);
	   break;
	}
    }

    /* examine return code */   
    string status = header_text.substr(header_text.find(" ") + 1);
    status = status.substr(0, status.find(" "));
    //Skip "HTTP/1.0"
    //remove the '\0'
    //Normal reply has return code 200
    if (status == "200") {
        ok = true;
    }
    else {
	ok = false;
    }

    /* print first part of response */
    fprintf(wheretoprint,header_text.c_str());
    fprintf(wheretoprint,endheaders);
    fprintf(wheretoprint,response.c_str());
    /* second read loop -- print out the rest of the response */
    while ((rc = minet_read(sock, buf, BUFSIZE)) > 0) {
        buf[rc] = '\0';
      if(ok) {        													    					      fprintf(wheretoprint, buf);            													    		  }
      else { 
        fprintf(stderr, buf);
      }
   }        													    						    /*char *whole = NULL;
    whole = (char*)malloc(sizeof(char) * 99999); //large enough to store whole http code
    while((rc = minet_read(sock, buf, BUFSIZE)) >0){
    	//buf[rc] = '\0';
	whole = strcat(whole, buf);
    	if (strstr(whole, endheaders)!= NULL)
        {
	     bptr = strtok(whole, endheaders);
	     bptr2 = strstr(whole, endheaders);
             //bptr2 += 4;
             break;
      	}
        //memset(buf, 0, rc);
    }*/
    
    /*close socket and deinitialize */
    minet_close(sock);
    minet_deinit();

    if (ok) {
	return 0;
    } else {
	return -1;
    }
}

int write_n_bytes(int fd, char * buf, int count) {
    int rc = 0;
    int totalwritten = 0;

    while ((rc = minet_write(fd, buf + totalwritten, count - totalwritten)) > 0) {
	totalwritten += rc;
    }
    
    if (rc < 0) {
	return -1;
    } else {
	return totalwritten;
    }
}

char *build_request_line(char *host, char *page)
{
  char *query;
  char *getpage = page;
  char *tpl = "GET /%s HTTP/1.0\r\nHost: %s\r\nConnection: close\r\nUser-Agent: %s\r\n\r\n";
  if(getpage[0] == '/'){
    getpage = getpage + 1;
    //fprintf(stderr,"Removing leading \"/\", converting %s to %s\n", page, getpage);
  }
  // -5 is to consider the %s %s %s in tpl and the ending \0
  query = (char *)malloc(strlen(host)+strlen(getpage)+strlen(USERAGENT)+strlen(tpl)-5);
  sprintf(query, tpl, getpage, host, USERAGENT);
  return query;
}
