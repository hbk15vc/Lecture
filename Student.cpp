#include <iostream>
#include <vector>
#include <memory>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>

class Student : public Person {
private:
    std::thread clientThread_;
    bool listening_;

public:
    // Constructor
    Student(const std::string& name, int age, const std::string& role = "Student")
        : Person(name, age, role), listening_(true) {}

    // Destructor
    ~Student() override {
        listening_ = false;
        if (clientThread_.joinable()) {
            clientThread_.join();
        }
    }

    // Start client for listening to server notifications
    void startListeningForNotifications() {
        clientThread_ = std::thread([this]() {
            int clientSocket;
            struct sockaddr_in serverAddr;
            char buffer[1024] = {0};

            clientSocket = socket(AF_INET, SOCK_STREAM, 0);
            if (clientSocket < 0) {
                std::cerr << "Error: Could not create client socket.\n";
                return;
            }

            serverAddr.sin_family = AF_INET;
            serverAddr.sin_port = htons(8080);
            serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

            if (connect(clientSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
                std::cerr << "Error: Connection failed.\n";
                close(clientSocket);
                return;
            }

            while (listening_) {
                int bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
                if (bytesRead > 0) {
                    buffer[bytesRead] = '\0';
                    std::cout << getName() << " received: " << buffer << "\n";
                }
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
            close(clientSocket);
        });
    }
};
