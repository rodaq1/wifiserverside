#pragma once

#include <string>

enum MessageType {
    BUTTON_PRESSED = 1
};

struct Message {
    MessageType type;
    int buttonIndex;

    std::string serialize() const {
        return std::to_string(type) + "," + std::to_string(buttonIndex);
    }

    static Message deserialize(const std::string &data) {
        Message message;
        size_t delimiter = data.find(',');
        message.type = static_cast<MessageType>(std::stoi(data.substr(0, delimiter)));
        message.buttonIndex = std::stoi(data.substr(delimiter + 1));
        return message;
    }
};