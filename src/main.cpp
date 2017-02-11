#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <thread>
#include <vector>
#include <queue>
#include <mutex>

std::queue<int> conn_queue;
std::mutex queue_mutex;
std::condition_variable cond_var;

void error(const char *msg) {
    perror(msg);
    exit(1);
}

void workerThread() {
    while (true) {
        int sockfd{0};
        printf("Waiting for work\n");

        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            cond_var.wait(lock, [](){ return !conn_queue.empty(); });
            sockfd = conn_queue.front();
            conn_queue.pop();
        }
        printf("Processing connection\n");

        char buffer[256];
        bzero(buffer,256);
        int n = read(sockfd, buffer,255);
        if (n < 0) error("ERROR reading from socket");
        printf("Here is the message: %s\n",buffer);
        n = write(sockfd, "I got your message", 18);
        if (n < 0) error("ERROR writing to socket");
        close(sockfd);
        printf("Finished talking to the client\n");
    }
}

int main(int argc, char *argv[]) {
    std::vector<std::thread> threads;
    const int N_THREADS = 4;
    for (int i = 0; i < N_THREADS; ++i) {
        threads.push_back(std::move(std::thread(workerThread)));
    }

    const int port = 4044;
    printf("Running the server on port %d...\n", port);

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        error("ERROR: open socket failed");
    }

    struct sockaddr_in serv_addr;
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port);
    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        error("ERROR: bind failed");
    }

    listen(sockfd, 5);

    while (true) {
        struct sockaddr_in cli_addr;
        socklen_t clilen = sizeof(cli_addr);
        int newsockfd = accept(sockfd, 
                    (struct sockaddr *) &cli_addr, 
                    &clilen);
        if (newsockfd < 0) {
            error("ERROR: accept failed");
        }
        printf("Client connected\n");

        std::unique_lock<std::mutex> lock(queue_mutex);
        conn_queue.push(newsockfd);
        cond_var.notify_one();
    }

    // enable when handling graceful shutdown
    // for (auto& th : threads) th.join();

    close(sockfd);   
    return 0;
}
