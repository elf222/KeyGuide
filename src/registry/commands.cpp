// registry/commands.cpp
#include "registry/commands.hpp"
#include "registry/global.hpp"

void CommandRegistry::executeAll()
{
    for (auto& command : commands)
    {
        command->execute();
    }
    clear();
}

void CommandRegistry::clear()
{
    commands.clear();
}

void DestroyEntity(Entity entity)
{
    cmd_registry.add<CommandDestroyEntity>(entity);
}
