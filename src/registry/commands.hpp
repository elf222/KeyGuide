// registry/commands.hpp
#pragma once
#include <memory>
#include <utility>
#include <vector>
#include "registry/ecs_components.hpp"

struct ICommand
{
    virtual ~ICommand() = default;
    virtual void execute() = 0;
};

struct CommandDestroyEntity : ICommand
{
    Entity e;

    explicit CommandDestroyEntity(Entity entity) : e(entity)
    {
    }

    void execute() override;
};

struct CommandInitialiseKeyboard : ICommand
{
    void execute() override;
};

struct CommandRebuildKeyboard : ICommand
{
    void execute() override;
};

struct CommandTrackInput : ICommand
{
    Platform::InputEvent event;

    explicit CommandTrackInput(Platform::InputEvent value) : event(value)
    {
    }

    void execute() override;
};

struct CommandSetAppProfile : ICommand
{
    Platform::AppProfile profile;
    bool macCommandAliases;

    explicit CommandSetAppProfile(Platform::AppProfile value, bool aliases = false)
        : profile(value), macCommandAliases(aliases)
    {
    }

    void execute() override;
};

struct CommandObserveBlender : ICommand
{
    Platform::BlenderObservation observation;

    explicit CommandObserveBlender(Platform::BlenderObservation value) : observation(value)
    {
    }

    void execute() override;
};

struct CommandFocusChanged : ICommand
{
    bool foreground;

    explicit CommandFocusChanged(bool value) : foreground(value)
    {
    }

    void execute() override;
};

struct CommandInvalidateState : ICommand
{
    const char* reason;

    explicit CommandInvalidateState(const char* value) : reason(value)
    {
    }

    void execute() override;
};

struct CommandSetDisplayHoverLock : ICommand
{
    bool inside;

    explicit CommandSetDisplayHoverLock(bool value) : inside(value)
    {
    }

    void execute() override;
};

struct CommandProjectKeyboard : ICommand
{
    Entity hovered;
    float expandedWidth, expandedHeight;

    CommandProjectKeyboard(Entity entity = -1, float width = 0, float height = 0)
        : hovered(entity), expandedWidth(width), expandedHeight(height)
    {
    }

    void execute() override;
};

struct CommandRegistry
{
    std::vector<std::unique_ptr<ICommand>> commands;

    template <typename T, typename... Args> void add(Args&&... args)
    {
        commands.push_back(std::make_unique<T>(std::forward<Args>(args)...));
    }

    void executeAll();
    void clear();
};

void DestroyEntity(Entity entity);
