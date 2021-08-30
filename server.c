#include <sys/socket.h>
#include <sys/stat.h>
#include <stdio.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <fcntl.h>
#include <unistd.h> // write
#include <string.h> // memset
#include <stdlib.h> // atoi
#include <stdbool.h> // true, false
#include <err.h>
#include <ctype.h>
#include "queue.h"
#include <pthread.h>

#define BUFFER_SIZE 2048 // might have to change to 16Kib
pthread_mutex_t healthcheck_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

struct httpObject {
    /*
        Create some object 'struct' to keep track of all
        the components related to a HTTP message
        NOTE: There may be more member variables you would want to add
    */
    char method[5];         // PUT, HEAD, GET
    char filename[28];      // what is the file we are worried about
    char httpversion[9];    // HTTP/1.1
    ssize_t content_length; // example: 13
    int status_code;
    uint8_t buffer[BUFFER_SIZE];
    char status_text[30];  
    int8_t reqFD;
    size_t buffer_elements;
    ssize_t client_sockd;
    int8_t log_FD;
    off_t* global_log_offset;
    off_t local_log_offset;
    off_t error_log_offset;
    char *header;
};

size_t logContentLength(struct httpObject* message,int*);
void read_http_response(ssize_t , struct httpObject*);
void process_request(ssize_t , struct httpObject*);
void construct_http_response(ssize_t , struct httpObject*);
void putReq(ssize_t , struct httpObject*);
void getReq(ssize_t , struct httpObject*);
bool isValid(char* ,char* , char* );
void *thread_func(void*);
unsigned char* string2Hex(unsigned char*,ssize_t,size_t);
//https://www.gnu.org/software/libc/manual/html_node/Example-of-Getopt.html
bool validArgs(int , char** ,char**, int *,char **);
bool healthCheck(struct httpObject*,int);

int main(int argc, char** argv) {
  
    char  *log_file,*port = argv[1];
    struct sockaddr_in server_addr;
    int numThreads;
    Queue queue = newQueue();

    if(validArgs(argc,argv,&port,&numThreads,&log_file)){
        
        pthread_t worker_threads[numThreads];
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(atoi(port));
        server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        socklen_t addrlen = sizeof(server_addr);
               

        for(int i = 0 ; i<numThreads;++i){
            pthread_create(&worker_threads[i],NULL,thread_func,&queue);
        }
        /*
            Create server socket
        */
        int server_sockd = socket(AF_INET, SOCK_STREAM, 0);

        // Need to check if server_sockd < 0, meaning an error
        if (server_sockd < 0) {
            perror("socket");
        }
        /*
            Configure server socket
        */
        int enable = 1;
        /*
            This allows you to avoid: 'Bind: Address Already in Use' error
        */
        int ret = setsockopt(server_sockd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));
        /*
            Bind server address to socket that is open
        */
        ret = bind(server_sockd, (struct sockaddr *) &server_addr, addrlen);

        /*
            Listen for incoming connections
        */
        ret = listen(server_sockd, SOMAXCONN); // 5 should be enough, if not use SOMAXCONN

        if (ret < 0) {
            return EXIT_FAILURE;
        }

        /*
            Connecting with a client
        */
        struct sockaddr client_addr;
        memset(&client_addr, 0, sizeof(client_addr));
        socklen_t client_addrlen=sizeof(client_addr);
        int8_t logFD=-2;
        off_t* offset = malloc(sizeof(off_t));
        *offset=0;
        if(log_file!=NULL){
            logFD=open(log_file,O_WRONLY|O_CREAT|O_TRUNC,S_IRWXU);
            if(logFD==-1)
                perror("Log file open1");
        }

        while(true){
            struct httpObject* message = malloc(sizeof(struct httpObject));
            memset(message, 0, sizeof(struct httpObject));
            message->log_FD=logFD;
            message->global_log_offset=offset;
          
            printf("[+] server is waiting...\n");
            /*
            * 1. Accept Connection
            */

            message->client_sockd= accept(server_sockd, &client_addr, &client_addrlen);
           
            if (message->client_sockd < 0) {
                perror("accept(Client Socket)");
                free(message);
                message=NULL;
            }

            else{
                printf("\nAccepted Connection...Dispatching to thread, CLIENT_FD: %ld\n",message->client_sockd);
                pthread_mutex_lock(&queue_mutex);
                enqueue(queue, message);
                pthread_mutex_unlock(&queue_mutex);
                pthread_cond_signal(&cond);
            }
        }
        freeQueue(&queue);
    }
    return EXIT_SUCCESS;
}

void *thread_func(void *queue_ptr){
    Queue queue =*(Queue*)queue_ptr;
    struct httpObject* message=NULL;
    while(true){
        pthread_mutex_lock(&queue_mutex);
        if(isEmpty(queue)){
            pthread_cond_wait(&cond, &queue_mutex);
        }
        message = (struct httpObject*)(back(queue));
        dequeue(queue);
        pthread_mutex_unlock(&queue_mutex);
        message->error_log_offset=-1;
        read_http_response(message->client_sockd, message);
        process_request(message->client_sockd, message);
        construct_http_response(message->client_sockd,message);
        if(message->log_FD>0){
            char* temp;
            if (message->status_code!=200 && message->status_code!=201){
                int length=strlen(message->header)+23;
                temp= malloc(sizeof(char)*(length)+1);
                length+=10;
                pthread_mutex_lock(&log_mutex);
                if(message->error_log_offset>=0)
                    *(message->global_log_offset)=message->error_log_offset;
                message->local_log_offset=*(message->global_log_offset);
                *(message->global_log_offset)+=length;
                pthread_mutex_unlock(&log_mutex);
                snprintf(temp,strlen(message->header)+24,"FAIL: %s --- response %u",message->header,message->status_code);
                message->local_log_offset+=pwrite(message->log_FD,temp,strlen(temp),message->local_log_offset);
                free(temp);
                temp=NULL;
            }
            temp=malloc(sizeof(char)*11);
            sprintf(temp,"\n========\n");
            message->local_log_offset+=pwrite(message->log_FD,temp,strlen(temp),message->local_log_offset);
            free(temp);
            temp=NULL;
        }
        printf("Response Sent\n");
        if(close (message->client_sockd)==-1)
            perror("httpserver close"); 

        if(message->header!=NULL)
            free(message->header);

        free(message);
        message=NULL;
    }
    return 0;

}

void read_http_response(ssize_t client_sockd, struct httpObject* message) {
    printf("This function will take care of reading message\n");
    char responseBuffer[BUFFER_SIZE],temp_method[BUFFER_SIZE],
        temp_filename[BUFFER_SIZE],temp_httpversion[BUFFER_SIZE],
        temp1[BUFFER_SIZE], temp2[BUFFER_SIZE],temp3[BUFFER_SIZE];
    char *str_tokenizer,*tempBuffer,* saveptr;
    int tokenCount;
    int16_t responseSize= recv(client_sockd,responseBuffer,BUFFER_SIZE-1,0);
   
    if(responseSize<0){
        perror("RESPONSE RECV FAILED:");
        message->status_code=500;
        strcpy(message->status_text,"Internal Server Error");
    }

    else{
        message->status_code=400;
        strcpy(message->status_text,"Bad Request");
        responseBuffer[responseSize]='\0';
        tempBuffer=strstr(responseBuffer,"\r\n\r\n"); 

        if(tempBuffer==NULL){
            str_tokenizer=strtok_r(responseBuffer,"\r\n",&saveptr);
            message->header=malloc(sizeof(char)*(strlen(str_tokenizer)+1));
            strcpy(message->header,str_tokenizer);
            return;
        }
        else if(strlen(tempBuffer)>4){
            message->buffer_elements=strlen(tempBuffer+4);
            memcpy(message->buffer,tempBuffer+4, message->buffer_elements);
        }
       
        *(tempBuffer)='\0';

        str_tokenizer=strtok_r(responseBuffer,"\r\n",&saveptr);
        message->header=malloc(sizeof(char)*(strlen(str_tokenizer)+1));//must be freed
        strcpy(message->header,str_tokenizer);
        tokenCount=sscanf(str_tokenizer, "%s %s %s", temp_method,temp_filename,temp_httpversion);
        if(tokenCount<3)
            return;  
        str_tokenizer=strtok_r(NULL,"\r\n",&saveptr);

        while(str_tokenizer!=NULL){
            if(sscanf(str_tokenizer,"%[^:] %2[: ] %s",temp1,temp2,temp3)!=3)
                return;
            else if(strcmp(temp2,": ")!=0)
                return;
            else if(strcmp(temp1,"Content-Length")==0){
                if(atoi(temp3)<0)
                    return;
                else{
                    message->content_length=atoi(temp3);
                }
            }
            str_tokenizer=strtok_r(NULL,"\r\n",&saveptr);
        }
        //printf("\nmethod:%s filename:%s http:%s con:%ld\n",temp_method,temp_filename,temp_httpversion,message->content_length);
        if(strcmp(temp_method,"GET")==0||strcmp(temp_method,"HEAD")==0){
            if(message->content_length>0)
                return;
        }
       
        if(isValid(temp_method,temp_filename,temp_httpversion)){
            strncpy(message->filename,temp_filename+1,strlen(temp_filename)+1);
            strncpy(message->httpversion,temp_httpversion,strlen(temp_httpversion)+1);
            strncpy(message->method,temp_method,strlen(temp_method)+1);
            message->status_code=0;
            strcpy(message->status_text,"");
        }
    }

}
/*
    \brief 2. Want to process the message we just recieved
*/
void process_request(ssize_t client_sockd, struct httpObject* message) {
    
    printf("Processing Request\n"); 
    bool health_flag= healthCheck(message,1);
    
    if(message->status_code!=0 && !health_flag){
        printf("\nstat:\n%d",message->status_code);
        message->content_length=0;
        return;
    }
    
    if(!health_flag){
        if(strcmp(message->method,"GET")==0 ||strcmp(message->method,"HEAD")==0){
            struct stat st;
            stat(message->filename, &st);
           
            if(stat(message->filename, &st)<0){
                perror("Process_req:");
                message->status_code=404;
                strcpy(message->status_text,"Not Found");
                return;
            }
           
            if((st.st_mode & S_IRUSR) == 0){
                message->status_code=403;
                strcpy(message->status_text,"Forbidden");
                return;
            }
            
            if(strcmp(message->method,"GET")==0)
                message->reqFD  = open(message->filename,O_RDONLY);
            else if(access(message->filename,R_OK)<0)
                message->reqFD=-1;
           
            if(message->reqFD <0){
                perror("process_request:GET/HEAD:");
                message->status_code=404;
                strcpy(message->status_text,"Not Found");
                return;
            }
            else{
                message->content_length = st.st_size;
                message->status_code=200;
                strcpy(message->status_text,"OK");
            }
        }
    }
    
    if(message->log_FD>0){
        int headLength=0;
        pthread_mutex_lock(&log_mutex);
        message->error_log_offset=message->local_log_offset=*(message->global_log_offset);
        if(strcmp(message->method,"GET")==0||strcmp(message->method,"PUT")==0)
            *(message->global_log_offset)+=logContentLength(message,&headLength)-1;
        else{
            logContentLength(message,&headLength);
            *(message->global_log_offset)+=headLength+9;
        }    
        pthread_mutex_unlock(&log_mutex);
        char *temp= malloc((headLength+1)*sizeof(char));
        snprintf(temp,headLength,"%s /%s length %lu",message->method,message->filename,message->content_length);
        message->local_log_offset+=pwrite(message->log_FD,temp,strlen(temp),message->local_log_offset);
        free(temp);
    }
 
    if(!health_flag && strcmp(message->method,"PUT")==0){
        putReq(client_sockd,message);
    }
}

size_t logContentLength(struct httpObject* message,int* length){
    size_t headerLen= strlen(message->method)+strlen(message->filename)+11;
    printf("%lu %lu",strlen(message->method),strlen(message->filename));
    int count= message->content_length/10;
    size_t hex_content_length;
    headerLen++;
    while(count!=0){
        count/=10;
        headerLen++;
    }
    *length=headerLen;
    if(message->content_length%20==0){
        if(message->content_length==0)
            hex_content_length= 0; 
        else if(message->content_length<=20)
            hex_content_length= 9; 
        else
            hex_content_length = ((message->content_length/20))*9;
    }

    else
        hex_content_length = ((message->content_length/20)+1)*9;

    hex_content_length+=message->content_length*3;

    return headerLen+hex_content_length+10;
}
/*
    \brief 3. Construct some response based on the HTTP request you recieved
*/
void construct_http_response(ssize_t client_sockd, struct httpObject* message) {
    printf("Constructing Response\n");  
    dprintf(client_sockd,"HTTP/1.1 %d %s\r\nContent-Length: %zd\r\n\r\n",
        message->status_code,message->status_text,message->content_length);

    if(strcmp(message->method,"GET")==0 && message->status_code == 200 )
        getReq(client_sockd,message);
    if(message->status_code!=200 && message->status_code!=201)
        healthCheck(message,2);

}

bool healthCheck(struct httpObject* message,int mode){
    static int errors;
    static int log_entries;
    bool flag=false;
    pthread_mutex_lock(&healthcheck_mutex);
    switch(mode){
        case 1:
            if(strcmp(message->method,"GET")==0 &&strcmp(message->filename,"healthcheck")==0 &&message->log_FD>0){
                message->buffer_elements=sprintf((char*)(message->buffer),"%01d\n%01d",errors,log_entries);
                message->content_length= message->buffer_elements;
                message->status_code=200;
                strcpy(message->status_text,"OK");
                log_entries++;
                flag= true;
                
            }   
            else{
                log_entries++;
                if(strcmp(message->filename,"healthcheck")==0){
                    if(strcmp(message->method,"GET")==0){
                        message->content_length=0;
                        message->status_code=404;
                        strcpy(message->status_text,"Not Found");
                    }
                    else if(strcmp(message->method,"PUT")==0 || strcmp(message->method,"HEAD")==0){
                        message->content_length=0;
                        message->status_code=403;
                        strcpy(message->status_text,"Forbidden");
                    }
                    flag= true;
                }
            }
            break;
        case 2:
            errors++;
            break;
        default:
            break;
    }
    pthread_mutex_unlock(&healthcheck_mutex);
return flag;
}
void putReq(ssize_t client_sockd, struct httpObject* message){
    ssize_t recvSize,remainder=message->content_length; 
    unsigned char* temp1;
    size_t offset=0;
    message->content_length=0;

    message->reqFD = open(message->filename,O_WRONLY |O_CREAT|O_TRUNC,S_IRWXU); 
    message->status_code=500;
    strcpy(message->status_text,"Internal Server Error");
    if(message->reqFD <0){
        perror("PUT open error");
        return;
    }
    while(remainder>0){  
        if(strlen((const char*)message->buffer)==0){
            recvSize= recv(client_sockd,message->buffer,BUFFER_SIZE,0);
            if(recvSize<0){
                perror("PUT RECV Error");
                return;
            }
        }
        else
            recvSize=message->buffer_elements;
        
        if(write(message->reqFD ,message->buffer,recvSize)<0){
            perror("PUT write");
            return;
        }
        temp1 = string2Hex((unsigned char*)message->buffer,recvSize,offset);
        message->local_log_offset+=pwrite(message->log_FD,temp1,strlen((char*)(temp1)),message->local_log_offset);
        offset+=recvSize;
        if(temp1!=NULL && recvSize>0)
            free(temp1);
        temp1=NULL;

        remainder-=recvSize;
        memset(message->buffer,0,BUFFER_SIZE);
        message->buffer_elements=0;
    }
    if(close (message->reqFD )==-1){
        perror("PUT close error:");
        return;
    }
    message->status_code=201;
    strcpy(message->status_text,"Created");
}
//Get response, sends files asked for in req 
void getReq(ssize_t client_sockd, struct httpObject* message){
    ssize_t remainder=message->content_length,sendSize;
    unsigned char* temp1;
    size_t offset=0;

    while (remainder>0){
        if(message->buffer_elements==0){
            sendSize= read(message->reqFD,message->buffer,BUFFER_SIZE);
            message->buffer_elements=0;
        }
        else
            sendSize=message->buffer_elements;
        if(sendSize<0){
            perror("GET read Error:");
            message->status_code=500;
            strcpy(message->status_text,"Internal Server Error");
            break;
        }
        if(send(client_sockd,message->buffer,sendSize,0)<0){
            perror("GET send Error:");
            message->status_code=500;
            strcpy(message->status_text,"Internal Server Error");
            break;
        }
      
        temp1 = string2Hex((unsigned char*)message->buffer,sendSize,offset);
        message->local_log_offset+=pwrite(message->log_FD,temp1,strlen((char*)(temp1)),message->local_log_offset);
        offset+=sendSize;
        memset(message->buffer,0,BUFFER_SIZE);
        remainder-=sendSize;
        if(temp1!=NULL)
            free(temp1);
        temp1=NULL;
    }
    if(message->reqFD>0 && close (message->reqFD )==-1)
        perror("GET close error");

}
//Check if request formatted properly
bool isValid(char* method,char* filename, char* httpversion){
    if(strcmp(method,"GET")!=0 && strcmp(method,"PUT")!=0 &&strcmp(method,"HEAD")!=0){
        return false;
    }
    if(strcmp(httpversion,"HTTP/1.1")!=0){
        return false;
    }
    if(strlen(filename)>28 ||strlen(filename)<=1) {
        return false;
    }
    if(filename[0]!='/'){
        return false;
    }
    for(size_t j=1;j<strlen(filename);j++){
        if(isalnum(filename[j])==0 &&filename[j]!='-' && filename[j]!='_')
            return false;      
    }
    return true;
}

unsigned char* string2Hex(unsigned char* str,ssize_t length,size_t offset){
    size_t index,hex_index=0;
    if(length<=0)
        return NULL;
    int rowCount;
    size_t totalSize;
    if((offset+length%20)==0){
      rowCount = ((length/20))*9;
    }
    else
        rowCount = ((length/20)+1)*9;
    totalSize= (length*3)+rowCount+1;
    unsigned char *hex=malloc(sizeof(unsigned char)*(totalSize));
  
    for(index = 0;index<(size_t)length;index++){
        if((index+offset)%20==0){
            snprintf((char*)(hex+hex_index),10,"\n%08ld",index+offset);
            hex_index+=9;
        }
        snprintf((char*)(hex+hex_index),4," %02x",str[index]);
        hex_index+=3;
    }
    return hex;
}
bool validArgs(int argc, char** argv,char** port, int *numThreads,char ** log_file){
    int c;
    char* tempStr=NULL;
    *numThreads = 4;
    *log_file = NULL;
    bool thread_flag=false,log_flag=false,port_flag;
    if(argc<2)
        return false;

    while ((c = getopt (argc, argv, "N:l:")) != -1){
        
        switch (c){
        case 'N':
            if(!thread_flag){
                tempStr = optarg;
                for(size_t t_index =0;t_index<strlen(tempStr);t_index++){
                    if(!isdigit(tempStr[t_index])){
                        printf("Thread arg should be an integer\n");
                        return false;
                    }
                } 
                if(atoi(tempStr)<=0){
                    printf("Thread arg should be an integer greater than 0\n");
                    return false; 
                }
                *numThreads = atoi(tempStr);
                thread_flag=true;
            }
            else{
                printf("More than one thread arg\n");
                return false; 
            }
            break;
        case 'l':
            if(!log_flag){
                *log_file = optarg;
                log_flag = true;
            }
            else{
                printf("More than one log file arg\n");
                return false; 
            }
            break;
        case '?':
            if (optopt == 'N' || (optopt == 'l')){
                 fprintf (stderr, "Option -%c requires an argument.\n", optopt);
                return false;
            }
            else {
                fprintf (stderr, "Unknown option `-%c'.\n", optopt);
                return false;
            }
        default:
            abort ();
        }
    }
   
    for (; optind < argc; optind++){
        port_flag=true;
        tempStr = argv[optind];
        for(size_t port_index =0;port_index<strlen(tempStr);port_index++){
            if(!isdigit(tempStr[port_index])){
                port_flag=false;
                break;
            }
        }
        //printf("%s\n",*port);
        if(port_flag==false){
            printf ("Non-option argument %s\n", argv[optind]);
            return false;
        }
        else
            *port=tempStr;
      
    }
    if(atoi(tempStr)>65535 || atoi(tempStr)<0||!port_flag){
        printf("Invalid Port/ Args Specified\n");
            return false;
    }
    printf("Port:%s Threads:%d Log_file:%s\n",*port,*numThreads,*log_file);
    return true;
}
