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

class Person {
private:
    std::string name_;
    int age_;
protected:
    std::string role_;
public:
    // Constructor
    Person(const std::string& name, int age, const std::string& role = "")
        : name_(name), age_(age), role_(role) {}

    // Destructor
    virtual ~Person() {}

    // Getters
    std::string getName() const { return name_; }
    int getAge() const { return age_; }

    // Setters
    void setName(const std::string& name) { name_ = name; }
    void setAge(int age) { age_ = age; }

    // Operator Overloads
    bool operator==(const Person& other) const {
        return name_ == other.name_ && age_ == other.age_;
    }

    bool operator=(const Person& other) {
        if (this != &other) {
            name_ = other.name_;
            age_ = other.age_;
            role_ = other.role_;
        }
        return true;
    }

    bool operator>(const Person& other) const {
        return age_ > other.age_;
    }

    bool operator<(const Person& other) const {
        return age_ < other.age_;
    }

    // toString Method
    virtual std::string toString() const {
        std::ostringstream oss;
        oss << "Name: " << name_ << ", Age: " << age_ << ", Role: " << role_;
        return oss.str();
    }
};

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

int main() {
    // Create some students
    Student student1("John Doe", 20);
    Student student2("Jane Smith", 22);

    // Create a lecture with a capacity of 10 students
    Lecture lecture("C++ Programming", "An advanced C++ programming class", 10);

    // Start listening for notifications for each student
    student1.startListeningForNotifications();
    student2.startListeningForNotifications();

    // Add students to the lecture
    lecture.addStudent(student1);
    lecture.addStudent(student2);

    // Save students to a file
    lecture.saveToFile("students.txt");

    // Restore students from a file and compare
    lecture.restoreFromFile("students.txt");
    lecture.compareAndRestoreFromFile("students.txt");

    // Print students in lecture
    lecture.printStudents();

    return 0;
}