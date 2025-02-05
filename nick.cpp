#include "Server.hpp"

void Server::broadcastNickChange(Client &client, const std::string &oldNick, const std::string &newNick)
{
    std::string message = ":" + oldNick + "!" + client.getUsername() + "@" + client.getHostName() + " NICK :" + newNick + "\r\n";

    for (std::map<int, Client>::iterator it = _clients.begin(); it != _clients.end(); ++it)
        sendReply(it->second.getClientFd(), message);
}

bool isValidNickname(std::string nickname)
{
    for (size_t i = 0; i < nickname.size(); i++)
    {
        if (!isalnum(nickname[i]) && nickname[i] != '_' && nickname[i] != '-')
            return false;
    }
    return true;
}

void Server::NickCommand(int client_fd, std::vector<std::string> command)
{
    if (command.size() < 2)
    {
        sendReply(client_fd, ERR_NONICKNAMEGIVEN(_clients[client_fd].getNickname(), _clients[client_fd].getHostName()));
        return;
    }
    Client &currClient = _clients[client_fd];
    if (currClient.getAuthStatus() < 0x01)
        return;

    std::string nickname = command[1];

    if (!isValidNickname(nickname))
    {
        sendReply(client_fd, ERR_ERRONEUSNICKNAME(currClient.getNickname(), nickname, currClient.getHostName()));
        return;
    }

    for (std::map<int, Client>::iterator it = _clients.begin(); it != _clients.end(); ++it)
    {
        if (it->second.getNickname() == nickname)
        {
            // std::string custom_error_message = ": 555 * " + nickname + " :Nickname already taken, please try again\r\n";
            // sendReply(client_fd, custom_error_message);
            sendReply(client_fd, ERR_NICKNAMEINUSE(nickname));
            return;
        }
    }

    std::string oldNick = currClient.getNickname();

    currClient.setAuthStatus(0x02);
    currClient.setNickname(nickname);

    if (currClient.getAuthStatus() == 0x07 && currClient.getNickFlag() == 0)
    {
        sendWelcomeMessages(client_fd, currClient);
        currClient.setNickFlag(1);
    }

    if (!oldNick.empty())
        broadcastNickChange(currClient, oldNick, nickname);
}
