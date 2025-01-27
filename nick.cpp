#include "Server.hpp"

void Server::NickCommand(int client_fd, std::vector<std::string> command)
{
    if (command.size() < 2)
    {
        sendReply(client_fd, ERR_NONICKNAMEGIVEN(_clients[client_fd].getNickname(), _clients[client_fd].getHostName()));
        return;
    }
    Client &currClient = _clients[client_fd];
    // if (currClient.getAuthStatus() != 0x01 && currClient.getAuthStatus() != 0x05 
    //     && currClient.getAuthStatus() != 0x07 && currClient.getAuthStatus() != 0x03)
    //     return;

    std::string nickname = command[1];
    for (std::map<int, Client>::iterator it = _clients.begin(); it != _clients.end(); ++it)
    {
        if (it->second.getNickname() == nickname)
        {
            sendReply(client_fd, ERR_NICKNAMEINUSE(currClient.getNickname(), nickname));
            return;
        }
    }
    // // std::cout << "Nickname set to " << nickname << " for client " << client_fd << std::endl;
    currClient.setAuthStatus(0x02);

    

    if (currClient.getAuthStatus() == 0x07 && currClient.getNickFlag() == 0 && currClient.getNickname().empty())
    {
        currClient.setNickname(nickname);
        sendWelcomeMessages(client_fd, currClient);
        currClient.setNickFlag(1);
    }
    currClient.setNickname(nickname);
}