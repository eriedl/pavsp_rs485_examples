//
// Created by Erhard Riedl on 6/7/16.
//

#ifndef RS485_MASTER_COMMANDQUEUE_H
#define RS485_MASTER_COMMANDQUEUE_H

struct CommandStruct
{
    uint8_t *command;
    size_t size;
};

struct CommandNode
{
    CommandStruct *commandInfo;
    CommandNode *nextNode;
};

class CommandQueue
{
public:
    CommandQueue();
    uint32_t Enqueue(CommandStruct *commandInfo);
    CommandStruct *Dequeue();
    const CommandStruct *Peek();
    uint32_t GetSize();
    boolean HasNext();
    void Clear();

private:
    CommandNode *frontNode;
    CommandNode *rearNode;
    uint32_t size;
};

#endif //RS485_MASTER_COMMANDQUEUE_H
