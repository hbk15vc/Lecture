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

class Lecture {
private:
    std::string name_;
    std::string description_;
    int capacity_;
    std::vector<std::unique_ptr<Student>> students_;
    std::thread serverThread_;
    std::mutex mtx_;
    std::condition_variable cv_;
    int serverSocket_;
    bool serverRunning_;

public:
    // Constructor
    Lecture(const std::string& name, const std::string& description, int capacity)
        : name_(name), description_(description), capacity_(capacity), serverRunning_(true) {
        startServer();
    }

    // Destructor
    ~Lecture() {
        serverRunning_ = false;
        if (serverThread_.joinable()) {
            serverThread_.join();
        }
        close(serverSocket_);
    }

    // Start the server thread for notifications
    void startServer() {
        serverThread_ = std::thread([this]() {
            struct sockaddr_in serverAddr;
            serverSocket_ = socket(AF_INET, SOCK_STREAM, 0);
            if (serverSocket_ == -1) {
                std::cerr << "Error: Could not create socket.\n";
                return;
            }

            serverAddr.sin_family = AF_INET;
            serverAddr.sin_addr.s_addr = INADDR_ANY;
            serverAddr.sin_port = htons(8080);

            if (bind(serverSocket_, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
                std::cerr << "Error: Bind failed.\n";
                return;
            }

            listen(serverSocket_, 3);
            std::cout << "Server started and listening on port 8080...\n";

            while (serverRunning_) {
                int clientSocket;
                struct sockaddr_in clientAddr;
                socklen_t clientSize = sizeof(clientAddr);
                clientSocket = accept(serverSocket_, (struct sockaddr*)&clientAddr, &clientSize);
                if (clientSocket < 0) {
                    std::cerr << "Error: Accept failed.\n";
                    continue;
                }

                std::thread(&Lecture::handleClient, this, clientSocket).detach();
            }
        });
    }

    // Handle client connections
    void handleClient(int clientSocket) {
        char buffer[1024];
        while (serverRunning_) {
            std::string message = "Notification: Lecture has " + std::to_string(students_.size()) +
                                  "/" + std::to_string(capacity_) + " students registered.";
            send(clientSocket, message.c_str(), message.size(), 0);
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        close(clientSocket);
    }

    // Add student to lecture
    void addStudent(const Student& student) {
        std::unique_lock<std::mutex> lock(mtx_);
        if (students_.size() < capacity_) {
            students_.push_back(std::make_unique<Student>(student));
            notifyStudents();
        } else {
            std::cerr << "Error: Lecture capacity reached.\n";
        }
    }

    // Notify all students when capacity thresholds are met
    void notifyStudents() {
        int thresholds[] = {25, 50, 75, 90};
        for (int threshold : thresholds) {
            if (students_.size() >= (capacity_ * threshold / 100)) {
                std::cout << "Lecture reached " << threshold << "% of capacity: " 
                          << students_.size() << "/" << capacity_ << "\n";
                break;
            }
        }
    }
};
