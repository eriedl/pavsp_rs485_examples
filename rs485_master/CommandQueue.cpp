//
// Created by Erhard Riedl on 6/7/16.
//

#include "Arduino.h"
#include "CommandQueue.h"

CommandQueue::CommandQueue()
{
    this->size = 0;
    this->frontNode = NULL;
    this->rearNode = NULL;
}

uint32_t CommandQueue::Enqueue(CommandStruct *commandInfo)
{
    // First element in queue: Front is the same as rear!, nextNode is not set
    if (this->frontNode == NULL)
    {
        this->frontNode = (CommandNode *)malloc(sizeof(CommandNode));
        this->frontNode->nextNode = NULL;
        this->frontNode->commandInfo = commandInfo;
        this->rearNode = this->frontNode;
    }
    else
    {
        CommandNode *tmp = (CommandNode *)malloc(sizeof(CommandNode));
        tmp->nextNode = NULL;
        tmp->commandInfo = commandInfo;

        // Create the link to the new node: Update the currently last node to point to the new node
        this->rearNode->nextNode = tmp;

        // Now make the new node the new rear node
        this->rearNode = tmp;
    }

    return ++this->size;
}

CommandStruct *CommandQueue::Dequeue()
{
    // Nothing in the queue, return the very same
    if (this->frontNode == NULL)
    {
        return NULL;
    }

    CommandStruct *ret = this->frontNode->commandInfo;

    if (this->frontNode->nextNode == NULL)
    {
        free(this->frontNode);  // Only need to free the memory of front as rear is the same as front now
        this->frontNode = NULL; // But we need to reset both pointers
        this->rearNode = NULL;
    }
    else
    {
        CommandNode *tmpNode = this->frontNode->nextNode;
        free(this->frontNode);     // Free memory used by front node
        this->frontNode = tmpNode; // Make the next node the new front node
    }

    --this->size;

    return ret;
}

const CommandStruct *CommandQueue::Peek()
{
    if (this->frontNode == NULL)
    {
        return NULL;
    }
    else
    {
        return this->frontNode->commandInfo;
    }
}

uint32_t CommandQueue::GetSize()
{
    return this->size;
}

void CommandQueue::Clear()
{
    while (this->HasNext() == true)
    {
        CommandStruct *cmd = this->Dequeue();
        free(cmd->command);
        free(cmd);
    }
}

boolean CommandQueue::HasNext()
{
    return (this->frontNode != NULL);
}
