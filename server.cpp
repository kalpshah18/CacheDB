#include <asio.hpp>
#include <iostream>
#include <unordered_map>
#include <memory>
#include <vector>
#include <sstream>
#include <array>
#include <fstream>
#include <chrono>
#include <ctime>
#include <iomanip>

using asio::ip::tcp;

std::unordered_map<std::string, std::string> kvStore;
std::chrono::steady_clock::time_point backupTime = std::chrono::steady_clock::now();

void saveBackup() {
    try {
        // Create timestamp for backup filename
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << "backup_" << std::put_time(std::localtime(&time_t_now), "%Y%m%d_%H%M%S") << ".dat";
        
        std::ofstream backupFile(ss.str());
        if (!backupFile.is_open()) {
            std::cerr << "Failed to create backup file: " << ss.str() << std::endl;
            return;
        }
        
        // Save the number of key-value pairs
        backupFile << kvStore.size() << "\n";
        
        // Save each key-value pair
        for (const auto& pair : kvStore) {
            backupFile << pair.first.length() << "\n" << pair.first << "\n";
            backupFile << pair.second.length() << "\n" << pair.second << "\n";
        }
        
        backupFile.close();
        std::cout << "Backup saved to: " << ss.str() << " (" << kvStore.size() << " keys)" << std::endl;
        
        // Update backup time
        backupTime = std::chrono::steady_clock::now();
    } catch (const std::exception& e) {
        std::cerr << "Error saving backup: " << e.what() << std::endl;
    }
}

void checkAutoBackup() {
    const auto backup_interval = std::chrono::minutes(5);
    auto now = std::chrono::steady_clock::now();
    
    if (now - backupTime >= backup_interval && !kvStore.empty()) {
        std::cout << "Performing automatic backup..." << std::endl;
        saveBackup();
    }
}

std::vector<std::string> parseRESP(const std::string& data){
    std::vector<std::string> result;
    size_t pos = 0;

    if(data[pos] != '*'){
        std::cerr << "Invalid RESP Array" << std::endl;
        return result;
    }
    pos++;
    size_t next = data.find("\r\n", pos);
    int num_elements = std::stoi(data.substr(pos,next - pos));
    pos = next + 2;

    for(int i = 0; i < num_elements; i++){
        if(data[pos] != '$'){
            std::cerr << "Expected Bulk String" << std::endl;
            return result;
        }
        pos++;
        next = data.find("\r\n", pos);
        int len = std::stoi(data.substr(pos, next - pos));
        pos = next + 2;
        
        std::string element = data.substr(pos, len);
        result.push_back(element);
        pos += len + 2;
    }
    return result;
}

std::string handleCommand(const std::vector<std::string>& cmd){
    if(cmd.empty()) return "-ERR empty command\r\n";
    if(cmd[0] == "PING") return "+PONG\r\n";
    if(cmd[0] == "SET"){
        if(cmd.size() != 3) return "-ERR wrong number of arguments for SET\r\n";
        kvStore[cmd[1]] = cmd[2];
        return "+OK\r\n";
    }
    if(cmd[0] == "GET"){
        if(cmd.size() != 2) return "-ERR wrong number of arguments for GET\r\n";
        auto it = kvStore.find(cmd[1]);
        if(it == kvStore.end()) return "$-1\r\n";
        std::ostringstream resp;
        resp << "$" << it->second.size() << "\r\n" << it->second << "\r\n";
        return resp.str();
    }
    if(cmd[0] == "BACKUP"){
        if(cmd.size() != 1) return "-ERR wrong number of arguments for BACKUP\r\n";
        saveBackup();
        return "+OK backup saved\r\n";
    }

    return "-ERR unknown command\r\n";
}

class Session : public std::enable_shared_from_this<Session> {
    public:
        Session(tcp::socket socket)
            : socket_(std::move(socket)) {}
        
        void start(){
            do_read();
        }
    
    private:
        void do_read(){
            auto self(shared_from_this());
            socket_.async_read_some(
                asio::buffer(data_),
                [this, self](std::error_code ec, std::size_t length){
                    if(!ec){
                        std::string input(data_.data(), length);
                        auto cmd = parseRESP(input);
                        std::string response = handleCommand(cmd);
                        
                        // Check for automatic backup after processing command
                        checkAutoBackup();
                        
                        do_write(response);
                    }
                }
            );
        }

        void do_write(const std::string& response){
            auto self(shared_from_this());
            asio::async_write(
                socket_,
                asio::buffer(response),
                [this, self](std::error_code ec, std::size_t /*length*/){
                    if(!ec){
                        do_read();
                    }
                }
            );
        }

        tcp::socket socket_;
        std::array<char, 4096> data_;
};

class Server {
    public:
        Server(asio::io_context& io_context, short port)
            : acceptor_(io_context, tcp::endpoint(tcp::v4(), port)){
                do_accept();
            }
    
    private:
            void do_accept(){
                acceptor_.async_accept(
                    [this](std::error_code ec, tcp::socket socket){
                        if(!ec){
                            std::make_shared<Session>(std::move(socket))->start();
                        }
                        do_accept();
                    }
                );
            }

            tcp::acceptor acceptor_;
};

int main(){
    try{
        asio::io_context io_context;
        int port = 6380;  // Using port 6380 instead of 6379
        Server server(io_context, port);
        std::cout << "CacheDB listening on port " << port << std::endl;
        std::cout << "Automatic backups will be created every 5 minutes when data is present" << std::endl;
        std::cout << "Use the BACKUP command to manually create a backup" << std::endl;
        io_context.run();
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
    return 0;
}