// registry/registry.hpp

#pragma once

#include <array>
#include <bitset>
#include <memory>
#include <utility>
#include <vector>
#include <cstddef>
#include <cassert>

#include "registry/ecs_components.hpp"

struct IComponentArray
{
    virtual ~IComponentArray() = default;
    virtual void removeEntity(Entity entity) = 0;
    virtual void clear() = 0;
};

template <typename T> struct ComponentArray : IComponentArray
{
    std::vector<T> data;
    std::vector<Entity> indexToEntity;
    std::vector<int> entityToIndex;

    void ensureEntityExists(Entity entity)
    {
        if (entity >= static_cast<Entity>(entityToIndex.size()))
        {
            entityToIndex.resize(entity + 1, -1);
        }
    }

    template <typename... Args> T& emplace(Entity entity, Args&&... args)
    {
        ensureEntityExists(entity);

        if (entityToIndex[entity] != -1)
        {
            T& component = data[entityToIndex[entity]];
            component = T(std::forward<Args>(args)...);
            return component;
        }

        int index = static_cast<int>(data.size());

        data.emplace_back(std::forward<Args>(args)...);
        indexToEntity.push_back(entity);
        entityToIndex[entity] = index;

        return data.back();
    }

    T* get(Entity entity)
    {
        if (entity < 0 || entity >= static_cast<Entity>(entityToIndex.size()))
        {
            return nullptr;
        }

        int index = entityToIndex[entity];

        if (index == -1)
        {
            return nullptr;
        }

        return &data[index];
    }

    const T* get(Entity entity) const
    {
        if (entity < 0 || entity >= static_cast<Entity>(entityToIndex.size()))
        {
            return nullptr;
        }

        int index = entityToIndex[entity];
        return index == -1 ? nullptr : &data[index];
    }

    bool has(Entity entity) const
    {
        if (entity < 0 || entity >= static_cast<Entity>(entityToIndex.size()))
        {
            return false;
        }

        return entityToIndex[entity] != -1;
    }

    void removeEntity(Entity entity) override
    {
        if (!has(entity))
        {
            return;
        }

        int removed_index = entityToIndex[entity];
        int last_index = static_cast<int>(data.size()) - 1;

        if (removed_index != last_index)
        {
            Entity last_entity = indexToEntity[last_index];

            data[removed_index] = std::move(data[last_index]);
            indexToEntity[removed_index] = last_entity;
            entityToIndex[last_entity] = removed_index;
        }

        data.pop_back();
        indexToEntity.pop_back();

        entityToIndex[entity] = -1;
    }

    void clear() override
    {
        data.clear();
        indexToEntity.clear();
        entityToIndex.clear();
    }
};

struct Registry
{
    Entity curEntity = 0;

    std::vector<Entity> freeEntities;

    std::array<std::unique_ptr<IComponentArray>, ComponentCount> components;
    std::vector<std::bitset<ComponentCount>> entityMasks;

    std::array<std::vector<Entity>, TagCount> tags;

    Registry();

    Entity createEntity();
    void destroyEntity(Entity entity);
    void clear();

    void addTag(Entity entity, TagName tag);
    void removeTag(Entity entity, TagName tag);
    bool hasTag(Entity entity, TagName tag);
    std::vector<Entity>& getTag(TagName tag);

    template <typename T> void registerComponent()
    {
        constexpr std::size_t INDEX = static_cast<std::size_t>(ComponentID<T>::VALUE);

        components[INDEX] = std::make_unique<ComponentArray<T>>();
    }

    constexpr std::size_t tagIndex(TagName tag)
    {
        return static_cast<std::size_t>(tag);
    }

    template <typename T> ComponentArray<T>& getArray()
    {
        constexpr std::size_t INDEX = static_cast<std::size_t>(ComponentID<T>::VALUE);

        assert(components[INDEX] && "Component type not registered");

        return *static_cast<ComponentArray<T>*>(components[INDEX].get());
    }

    template <typename T, typename... Args> T& emplaceComponent(Entity entity, Args&&... args)
    {
        T& component = getArray<T>().emplace(entity, std::forward<Args>(args)...);
        constexpr std::size_t INDEX = static_cast<std::size_t>(ComponentID<T>::VALUE);

        entityMasks[entity].set(INDEX);

        return component;
    }

    template <typename T, typename U> T& addComponent(Entity entity, U&& component)
    {
        return emplaceComponent<T>(entity, std::forward<U>(component));
    }

    template <typename T> T* getComponent(Entity entity)
    {
        return getArray<T>().get(entity);
    }

    template <typename T> bool hasComponent(Entity entity)
    {
        return getArray<T>().has(entity);
    }

    template <typename T> bool hasComponent(Entity entity) const
    {
        constexpr std::size_t INDEX = static_cast<std::size_t>(ComponentID<T>::VALUE);

        return entityMasks[entity].test(INDEX);
    }
};
