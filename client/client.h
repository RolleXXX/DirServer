#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <libgen.h>
#include <sys/stat.h>
#include <dirent.h>


#define BUF_SZ (1400)
#define LEN_OFFSET (BUF_SZ-5)
#define FILE_SZ (1024)



class Client{
    
    enum
    {
        UPLOAD = 1,
        DOWNLOAD,
    };

    enum
    {
        FNAME_FIN = 1,
        FNAME_NFIN,
        FCON_FIN,
        FCON_NFIN,
        CMD,
        PATH,
    };
    
public:
    
    Client(char *ip, int pt);
    int upload_file_to_server();
    int save_file_from_server();

    void setPath(char* path);
    void setServerpath(char *server_path);
    void setLocalpath(char *localpath);
    

private:
    int connect_server(char *serverip, int port);
    int check_warp(int len, char *addr);
    int sendfile(int confd, char *root_path, char *file, int if_fname_fin);
    int walkdir(char *path, char *files);
    void sendreg(int confd, char *file, int if_fname_fin);
    int recvfile(int sockfd, char *server_root, char *localpath, int *final_flg);
    int check_mkpath(char *dirpath);

private:
    char* path;//上传文件到服务器时本地文件的路径
    char *serverip;
    int port;
    char *server_path;
    char *localpath;
};
