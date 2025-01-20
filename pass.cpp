#include "Server.hpp"

void Server::PassCommand(int client_fd, std::vector<std::string> command)
{
    if (command.size() < 2)
    {
        std::cout << "Error: PASS command requires a password" << std::endl;
        return;
    }

    std::string password = command[1];
    Client &currClient = _clients[client_fd];
    // if (currClient.isRegistered())
    // {
    //     std::cout << "Error: Client is already registered" << std::endl;
    //     return;
    // }

    if (_password == password)
    {
        std::cout << "Password set for client " << currClient.getNickname() << std::endl;
        currClient.setAuthStatus(0x01);
    }
    else {
        std::cout << "Error : wrong pass\n";
    }
}