#include "Server.hpp"

void Server::channelTopic(Client& currClient, std::vector<std::string> command)
{
    if (command.size() < 2 || command.size() > 3)
    {
        std::cout << "Error: TOPIC <channel> [<topic>]\n";
        return;
    }

    std::string channelName = command[1];
    std::string newTopic = command[2];
    std::map<std::string, Channel>::iterator it;
    it = _channels.find(channelName);
    if (it == _channels.end())
    {
        std::cout << "Error: channel " << channelName << " does not exist" << std::endl;
        return;
    }
    else
    {
        Channel currChannel = it->second;
        if (currChannel.isOperator(currClient.getNickname()) == false)
        {
            std::cout << "Error: " << currClient.getNickname() << " is not an operator in channel " << channelName << std::endl;
            return;
        }
        if (command.size() == 2)
        {
            std::cout << "Topic for channel " << channelName << " is " << currChannel.getTopic() << std::endl;
        }
        else
        {
            currChannel.setTopic(newTopic);
            currChannel.setTopicDate(getCurrTime());
            currChannel.setTopicSetter(currClient.getNickname());
            std::cout << "Topic for channel " << channelName << " is set to " << newTopic << std::endl;
        }
    }

}

