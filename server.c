#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <string.h>

typedef struct {
    char *name;
    char *data;
} Header;

typedef struct {
    char *method;
    char *path;
    char *httpVersion;
    char *body;
    Header headers[100];
    char error[150];
} RequestData;

typedef struct {
    char* content;
    long length;
} Content;

// Global Variables
char projectDir[512];
char CLRF[] = "\r\n";


// Function Prototypes
void printHeaders(RequestData *data);
Content* readFile(char* path);
RequestData* parseRequest(char *request);
char* buildResponse(RequestData *data, long contLength, int statusCode, char *type);
void handleRequest(RequestData *request, int connfd);
char* getExtension(char *path);
char* getContentType(char *path);
char* get404Page(RequestData *data);

int main() {
    int seed = time(NULL);
    srand(seed);
    int sockfd, connfd, n, i;
    char request[1025];
    char *response;
    struct sockaddr_in serv_addr;
    RequestData *data;
    //   int port = 5000;
    int port = 5000 + rand() % 10;

    // get project dir
    getcwd(projectDir, sizeof (projectDir));

    // get socket descriptor
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("ERROR opening socket\n");
        return 1;
    }

    // build server_addr struct
    bzero((char *) &serv_addr, sizeof (serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port);
    printf("port: %d\n", port);

    // bind socket to serv_addr
    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof (serv_addr)) < 0) {
        printf("ERROR on binding\n");
        return 1;
    }

    // listen for connections to socket
    listen(sockfd, 5);

    while (1) {
        // accept connections from requests tail
        if ((connfd = accept(sockfd, (struct sockaddr *) NULL, NULL)) < 0) {
            printf("ERROR on accept\n");
            return 1;
        }

        bzero(request, 1024);

        // read request
        if (read(connfd, request, 1024) < 0) {
            printf("ERROR reading from socket\n");
            return 1;
        } else {
            // HANDLE REQUEST

            // fill request struct
            data = parseRequest(request);


            // check if request is valid
            if (data->method[0] == 0 && (strcmp(data->httpVersion, "HTTP/1.0") != 0 || strcmp(data->httpVersion, "HTTP/1.1") != 0) && data->path[0] != '/') {
                // return 400 status code or something else
            } else {
                handleRequest(data, connfd);
            }

            close(connfd);
        }
    }
    return 0;
}

void handleRequest(RequestData *request, int connfd) {
    Content *content;
    char *resp, *type;
    int statusCode = 200;

    if (strcmp(request->method, "GET") == 0) {
        content = readFile(request->path);
        type = getContentType(request->path);
        
        
        if (content->length == 0) {
            // return 404
            printf("400 page should appear\n");
            statusCode = 400;
            type = NULL;
        } else if (!type) {
            printf("404 page should appear\n");
            statusCode = 404;
            strcpy(type, "text/html");
            content = readFile("/404.html");
        }
        
        // build response status and headers
        resp = buildResponse(request, content->length, statusCode, type);


        // write response to client
        if (write(connfd, resp, strlen(resp)) == -1) {
            printf("ERROR on write\n");
            return;
        }
        
        // write body
        if(content->length > 0) {
            if (write(connfd, content->content, content->length) == -1) {
                printf("ERROR on write\n");
                return;
            }
        }
        free(content);
        free(request);
        free(resp);
        free(type);
    } else if (strcmp(request->method, "POST")) {

    } else if (strcmp(request->method, "PUT")) {

    } else if (strcmp(request->method, "DELETE")) {

    } else {
        // return 404 || 400
    }
}

char* buildResponse(RequestData *data, long contLength, int statusCode, char *type) {
    char *resp;
    int i = 0;
    char status[50];
    char contentType[50];
    char contentLength[50];

    resp = (char*) malloc(1024);
    bzero(resp, 1024);

    // add status
    sprintf(status, "HTTP/1.1 %d\r\n", statusCode);
    strcpy(&resp[i], status);
    i += strlen(status) - 1;
    
    // add headers
    if(contLength != 0 && type != NULL) {
        sprintf(contentLength, "Content-Length: %d\r\n", contLength);
        sprintf(contentType, "Content-Type: %s\r\n", type);
        
        strcpy(&resp[i], contentType);
        i += strlen(contentType) - 1;
        strcpy(&resp[i], contentLength);
        i += strlen(contentLength) - 1;
    }

    // split body from headers
    strcpy(&resp[i], CLRF);
    i += sizeof (CLRF) - 1;
    strcpy(&resp[i], CLRF);


    return resp;
}

void printHeaders(RequestData *data) {
    int i = 0;
    while (data->headers[i].name) {
        printf("Header: %s, data: %s\n", data->headers[i].name, data->headers[i].data);
        i++;
    }
}

RequestData* parseRequest(char *request) {
    RequestData* data = (RequestData*) malloc(sizeof (RequestData));
    char *pch = NULL;
    char *lines[100];
    char *err;
    int i = 0;

    // make every line to point NULL
    while (i < 100) {
        lines[i++] = NULL;
    }

    // split request to lines
    i = 0;
    pch = strtok(request, "\r\n");
    while (pch != NULL) {
        lines[i++] = pch;
        pch = strtok(NULL, "\r\n");
    }

    // get method, path and version
    pch = strtok(lines[0], " ");
    if (pch) {
        data->method = pch;
        pch = strtok(NULL, " ");
        if (pch) {
            data->path = pch;
            pch = strtok(NULL, " ");
            if (pch) {
                data->httpVersion = pch;
            } else {
                strcpy(data->error, "Cannot get protocol version");
                return data;
            }
        } else {
            strcpy(data->error, "Cannot get path");
            return data;
        }

    } else {
        strcpy(data->error, "Cannot get method");
        return data;
    }


    // get headers
    i = 1;
    while (lines[i]) {
        pch = strtok(lines[i], ": ");
        if (pch) {
            data->headers[i - 1].name = pch;
            pch = strtok(NULL, ": ");
            if (pch) {
                data->headers[i - 1].data = pch;
            } else {
                sprintf(err, "Invalid Header: %s", data->headers[i - 1].name);
                strcpy(data->error, err);
                break;
            }
        }
        i++;
    }

    return data;
}

Content* readFile(char* path) {
    FILE *fp;
    Content *cont;
    char fullPath[1024];

    cont = (Content*) malloc(sizeof (Content));

    // copy project dir
    strcpy(fullPath, projectDir);

    // add path to hosted website
    strcat(fullPath, "/website");

    // add request path
    strcat(fullPath, path);


    // open file for binary read
    fp = fopen(fullPath, "rb");

    if (fp) {
        // get length
        fseek(fp, 0, SEEK_END);
        cont->length = ftell(fp);
        fseek(fp, 0, SEEK_SET);


        // read file
        cont->content = (char*) malloc(cont->length);
        fread(cont->content, cont->length, 1, fp);
        fclose(fp);
    } else {
        printf("cannot open file for reading: %s\n", fullPath);
        cont->content = NULL;
        cont->length = 0;
        return cont;
    }

    return cont;
}

char* getExtension(char *path) {
    int pathLen = strlen(path), i, ind = -1, extLen;
    char *ext = NULL;

    for (i = pathLen - 2; i > 0; i--) {
        if (path[i] == '.') {
            ind = i;
            break;
        }
    }

    if (ind != -1) {
        extLen = pathLen - ind;
        if (extLen > 5) {
            return NULL;
        }
        ext = (char*) malloc(extLen);
        strcpy(ext, &path[ind + 1]);
    }

    return ext;
}

char* getContentType(char *path) {
    int pathLen = strlen(path), i, ind = -1, extLen;
    char *type = NULL, *ext;
    int typelen = 30;

    ext = getExtension(path);

    if (ext) {
        type = (char*) malloc(typelen);
        if (strcmp(ext, "jpg") == 0 || strcmp(ext, "jpeg") == 0) {
            strcpy(type, "image/jpg");
        } else if (strcmp(ext, "png") == 0) {
            strcpy(type, "image/png");
        } else if (strcmp(ext, "gif") == 0) {
            strcpy(type, "image/gif");
        } else if (strcmp(ext, "html") == 0 || strcmp(ext, "htm") == 0) {
            strcpy(type, "text/html; charset=UTF-8");
        } else if (strcmp(ext, "js") == 0) {
            strcpy(type, "text/javascript");
        } else if (strcmp(ext, "json") == 0) {
            strcpy(type, "application/json");
        } else if (strcmp(ext, "css") == 0) {
            strcpy(type, "text/css");
        } else if (strcmp(ext, "woff") == 0) {
            strcpy(type, "application/font-woff");
        } else if (strcmp(ext, "ttf") == 0) {
            strcpy(type, "application/x-font-ttf");
        } else if (strcmp(ext, "ico") == 0) {
            strcpy(type, "image/x-icon");
        } else {
            strcpy(type, "text/plain");
        }
    } else {
        return NULL;
    }

    return type;
}