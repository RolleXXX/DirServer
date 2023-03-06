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
#define BUF_SZ (1400)
#define LEN_OFFSET (BUF_SZ-5)
#define FILE_SZ (1400)
//server_path 是server上文件夹的绝对地址，localpath是下载到本地的地址
int save_file_from_server(char *serverip, int port, char *path, char *localpath);

//client_file_path是本地上传绝对路径，上传到server的指定路径下，server路径写死在逻辑中。
int upload_file_to_server(char *serverip, int port, char *path);
int check_mkpath(char *dirpath);

int main(int argc, char **argv)
{
    char *serverip = "127.0.0.1";
    int port = 8000;
    
    char localpath[] = "/home/u/C AND C++/DIrTrans/";//localpath是下载到本地的地址
    char server_path[] = "/root/Daima/Upload/img";//server上文件夹的绝对地址
    char client_file_path[] = "/home/u/C AND C++/DIrTrans/1.png";//上传文件到服务器时本地文件的路径

    save_file_from_server(serverip, port, server_path, localpath);
    upload_file_to_server(serverip, port, client_file_path);
    return 0;
}
int connect_server(char *serverip, int port){
    int sockfd;
    struct sockaddr_in servaddr;

    //创建socket
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        printf(" create socket error: %s (errno :%d)\n", strerror(errno), errno);
        return 0;
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    servaddr.sin_addr.s_addr = inet_addr(serverip);

    //连接
    if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
    {
        perror("connect ");
        return 0;
    }

    return sockfd;
}
int check_warp(int len,char* addr){
    int len_insddr = *(int*)addr;
    if(len!=len_insddr){
        printf("len %d,len in addr %d,NOT EQUAL!\n", len, len_insddr);
        return -1;
    }
    return 0;
}
int recvfile(int sockfd, char *server_root, char *localpath, int *final_flg)
{
    int recv_len,content_len;
    char filename[1024] = {0};
    char recv_buf[BUF_SZ] = {0};
    char *rela_path;

    recv_len = recv(sockfd, recv_buf, BUF_SZ, 0);
    if (recv_len < 0)
    {
        perror("recv");
        return -1;
    }

    *final_flg = recv_buf[BUF_SZ - 1];
    content_len = *(int*)(&recv_buf[LEN_OFFSET]);

    check_warp(content_len,recv_buf + LEN_OFFSET);

    if (!(*final_flg == FNAME_NFIN || *final_flg == FNAME_FIN))
    {
        printf("filename miss\n");
        return -1;
    }

    rela_path = recv_buf;
    if(strlen(rela_path)+1 != content_len){
        printf("file name wrong\n");
        return - 1;
    }
    rela_path += strlen(server_root);
    strncpy(filename, localpath, strlen(localpath) + 1);
    strncat(filename, rela_path, strlen(rela_path) + 1);
    printf("making %s\n", filename);

    char tmpdirname[1024] = {0};
    strncpy(tmpdirname, filename, strlen(filename) + 1);
    if (check_mkpath(dirname(tmpdirname)) != 0)
    {
        printf("Fail to save file\n");
    }

    FILE *fp = fopen(filename, "wb"); //以二进制方式打开（创建）文件
    if (fp == NULL)
    {
        printf("Cannot open file %s\n", filename);
        exit(0);
    }
    int content_flag = 0;
    while (recv_len = recv(sockfd, recv_buf, BUF_SZ, 0))
    {
        if(recv_len != BUF_SZ){
            //如果recv的长度不是BUF_SZ,意味着有一部分数据丢失了或者阻塞了（网络问题）
            printf("%s recv %d bytes\n",filename,recv_len);
            //统计现在接受到了多少数据
            int recv_part_len=recv_len;
            //这一步需要判断，防止下面数组越界。
            //为什么会出现接受的数据大于BUF_SZ？
            if(recv_part_len > BUF_SZ) {
                recv_part_len = BUF_SZ;
            }
            
            int BUF_should=BUF_SZ-recv_part_len;//统计还需要接受多少数据
            
            printf("%d\n",BUF_should);
            while((recv_len = recv(sockfd, recv_buf+recv_part_len, BUF_should, 0))){
                //循环往recv_buf里接受，直到把应该接受的数据接受完
                if(recv_len==BUF_should){
                    break;
                }
                else{
                    printf("%s recv %d bytes\n",filename,recv_len);
                    //调整水位继续接受
                    BUF_should-=recv_len;
                    recv_part_len+=recv_len;
                }
            }
        }
        content_len = *(int*)(&recv_buf[LEN_OFFSET]);
        content_flag = recv_buf[BUF_SZ - 1];

        fwrite(recv_buf, 1, content_len, fp);

        //check_warp(content_len,recv_buf + LEN_OFFSET);

        if(content_flag == FCON_FIN){
            break;
        }else if(content_flag == FCON_NFIN){
            continue;
        }else{
            printf("Unkown flag \n");
            return -1;
        }
    }

    fclose(fp);
    printf("recv file %s\n",filename);
}
//server_root根据实际来改
int save_file_from_server(char *serverip, int port, char *path, char *localpath)
{
    char recv_buf[BUF_SZ] = {0};
    char send_buf[BUF_SZ] = {0};
    int recv_len;
    int send_len;
    int sockfd = connect_server(serverip, port);


    char localfile[1024] = {0};
    char *fileName;
    char *server_root = "/root/Daima/Upload/img/";

    //send commod
    send_len = 1;
    send_buf[0] = DOWNLOAD;
    send_buf[BUF_SZ - 1] = CMD;

    strncpy(send_buf + LEN_OFFSET, (char*)&send_len, sizeof(int));
    check_warp(send_len,send_buf + LEN_OFFSET);

    if (send(sockfd, send_buf, BUF_SZ, 0) < 0)
    {
        perror("send cmd");
        return -1;
    }

    // send path
    send_len = strlen(path) + 1;
    strncpy(send_buf, path, strlen(path) + 1);
    send_buf[BUF_SZ - 1] = PATH;

    strncpy(send_buf + LEN_OFFSET, (char*)&send_len, sizeof(int));
    check_warp(send_len,send_buf + LEN_OFFSET);

    if (send(sockfd, send_buf, BUF_SZ, 0) < 0)
    {
        perror("send path");
        return 0;
    }

    //write data
    int flag = 0;
    do
    {
        recvfile(sockfd, server_root, localpath, &flag);
    } while (flag==FNAME_NFIN); 

    printf("recv all file \n");
    close(sockfd);
    return 0;
}
int check_mkpath(char *dirpath)
{
    char dir[1024] = {0};
    strncpy(dir, dirpath, strlen(dirpath) + 1);
    if (access(dirpath, F_OK) != 0)
    {
        if (mkdir(dirpath, S_IRWXU | S_IRWXG | S_IRWXO) != 0)
        {
            dirname(dir);
            check_mkpath(dir);
            mkdir(dirpath, S_IRWXU | S_IRWXG | S_IRWXO);
        }
        return access(dirpath, F_OK);
    }
    return 0;
}

int walkdir(char *path, char *files)
{
    DIR *dir;
    struct dirent *ptr;
    int len = 0;
    char deepPath[1024] = {0};
    char fileName[1024] = {0};
    if ((dir = opendir(path)) == NULL)
    {
        perror("error");
        return -1;
    }

    while ((ptr = readdir(dir)) != NULL)
    {
        if (strcmp(ptr->d_name, ".") == 0 || strcmp(ptr->d_name, "..") == 0) ///current dir OR parrent dir
            continue;
        else if (ptr->d_type == 10) ///link file,ignore
            continue;
        else if (ptr->d_type == 4) ///dir
        {
            strncpy(deepPath, path, strlen(path) + 1);
            strncat(deepPath, "/", 1 + 1);
            strncat(deepPath, ptr->d_name, strlen(ptr->d_name) + 1);
            walkdir(deepPath, files);
        }
        else if (ptr->d_type == 8) ///file
        {
            strncpy(fileName, path, strlen(path) + 1);
            strncat(fileName, "/", 1 + 1);
            strncat(fileName, ptr->d_name, strlen(ptr->d_name) + 1);

            if (strlen(files) + strlen(fileName) + 1 > FILE_SZ)
            {
                printf("too much files! fail to get file name \n");
                memset(files, 0, FILE_SZ);
                return -1;
            }
            strncat(files, fileName, strlen(fileName));
            strncat(files, "\n", 1 + 1);
        }
    }
    closedir(dir);
    return 1;
}

char* final_dir(char * path){
    char *tmp = path;
    char *final_dir=path;
    while (*tmp != 0)
    {
        if(*tmp =='/')
            final_dir = tmp;
        tmp++;
    }
    return final_dir + 1;
}
int sendfile(int confd,char* root_path, char *file, int if_fname_fin)
{
    int content_len;    
    char send_buf[BUF_SZ] = {0};

    FILE *fp = fopen(file, "rb");
    if(strlen(file)+1+5 >= BUF_SZ){
        printf("file name too long \n");
        return -1;
    }

    //send file name
    char* rela_path =file;
    rela_path += strlen(root_path);

    strncpy(send_buf, rela_path, strlen(rela_path) + 1);
    content_len = strlen(rela_path) + 1;
    send_buf[BUF_SZ - 1] = if_fname_fin;
    strncpy(send_buf + LEN_OFFSET, (char *)&content_len, sizeof(int));
    
    check_warp(content_len,send_buf + LEN_OFFSET);

    send(confd, send_buf,  BUF_SZ, 0);

    memset(send_buf, 0, BUF_SZ);
    while ((content_len = fread(send_buf, 1, BUF_SZ-5, fp)) >= 0)
    {
        if(content_len == BUF_SZ-5){
            send_buf[BUF_SZ - 1] = FCON_NFIN;
        }else{
            send_buf[BUF_SZ - 1] = FCON_FIN;
        }
        strncpy(send_buf + LEN_OFFSET, (char *)&content_len, sizeof(int));
        
        check_warp(content_len,send_buf + LEN_OFFSET);
        
        send(confd, send_buf, BUF_SZ, 0);
        

        if(send_buf[BUF_SZ - 1] == FCON_FIN)
            break;
    }

}

void sendreg(int confd,char* file,int if_fname_fin){

    int content_len;
    char send_buf[BUF_SZ] = {0};
    FILE *fp = fopen(file, "rb");
    if(strlen(file)+1+5 >= BUF_SZ){
        printf("file name too long \n");
    }
    send_buf[BUF_SZ - 1] = if_fname_fin;
    //发送文件的名字
    for (int i = strlen(file)-1; i >=0;--i){
        if('/'==file[i]){
            strncpy(send_buf, file + i, strlen(file) - i);
            send(confd, send_buf, BUF_SZ, 0);
            break;
        }
    }
    memset(send_buf, 0, BUF_SZ);
    while ((content_len = fread(send_buf, 1, BUF_SZ-5, fp)) >= 0)
    {
        if(content_len == BUF_SZ-5){
            send_buf[BUF_SZ - 1] = FCON_NFIN;
        }else{
            send_buf[BUF_SZ - 1] = FCON_FIN;
        }
        strncpy(send_buf + LEN_OFFSET, (char *)&content_len, sizeof(int));
        
        check_warp(content_len,send_buf + LEN_OFFSET);
        
        send(confd, send_buf, BUF_SZ, 0);

        

        if(send_buf[BUF_SZ - 1] == FCON_FIN)
            break;
    }

}

//path :本地需要上传的文件夹的位置
int upload_file_to_server(char *serverip, int port, char *path){
    int sockfd = connect_server(serverip, port);
    int send_len;
    char send_buf[BUF_SZ];
    char fileNames[FILE_SZ];

    //send commod
    send_len = 1;
    send_buf[0] = UPLOAD;
    send_buf[BUF_SZ - 1] = CMD;
    strncpy(send_buf + LEN_OFFSET, (char*)&send_len, sizeof(int));
    check_warp(send_len,send_buf + LEN_OFFSET);
    if (send(sockfd, send_buf, BUF_SZ, 0) < 0)
    {
        perror("send cmd");
        return -1;
    }
    
    //判断这个地址是一个文件夹
    struct stat buf;
    int result;
    result = stat(path, &buf);
    if(S_IFDIR&buf.st_mode){

        // 得到本地路径下所有文件
        walkdir(path, fileNames);
        //逐个发送路径内容
        char dirpath[1024] = {0};
        strncpy(dirpath, path, strlen(path) + 1);
        dirname(dirpath);

        char *tmp_ftail = fileNames;
        char *tmp_fhead = fileNames;
        while (*tmp_ftail != 0)
        {
            if (*tmp_ftail == '\n')
            {
                *tmp_ftail = 0;
                printf("send tmp_fhead %s \n", tmp_fhead);
                if (*(tmp_ftail+1) == 0){
                    sendfile(sockfd,dirpath, tmp_fhead, FNAME_FIN);
                }
                else{
                    sendfile(sockfd,dirpath, tmp_fhead, FNAME_NFIN);
                }
                tmp_fhead = tmp_ftail + 1;
            }
            ++tmp_ftail;
        }
    }
    else if(S_IFREG&buf.st_mode){
        char *tmp_ftail = path;
        // char *tmp_fhead = path;
        printf("send reg %s \n", path);
        sendreg(sockfd, path, FNAME_FIN);
    }


    close(sockfd);
}
