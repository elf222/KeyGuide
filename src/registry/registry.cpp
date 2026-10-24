// registry/registry.cpp

#include "registry/registry.hpp"
#include "registry/ecs_components.hpp"

Registry::Registry()
{
    registerComponent<Entity_Position>();
    registerComponent<Entity_Size>();
    registerComponent<Key_Layout>();
    registerComponent<Colour>();
    registerComponent<Key_Values>();
    registerComponent<Key_Hint>();
    registerComponent<Key_Display>();
    registerComponent<Tracked_State>();
}

Entity Registry::createEntity()
{
    Entity entity;

    if (!freeEntities.empty())
    {
        entity = freeEntities.back();
        freeEntities.pop_back();
    }
    else
    {
        entity = curEntity++;
    }

    if (entity >= static_cast<Entity>(entityMasks.size()))
    {
        entityMasks.resize(entity + 1000);
    }

    entityMasks[entity].reset();

    return entity;
}

void Registry::destroyEntity(Entity entity)
{
    if (entity < 0 || entity >= static_cast<Entity>(entityMasks.size()))
    {
        return;
    }

    for (auto& component_array : components)
    {
        if (component_array)
        {
            component_array->removeEntity(entity);
        }
    }

    entityMasks[entity].reset();

    for (std::size_t i = 0; i < TagCount; ++i)
    {
        removeTag(entity, static_cast<TagName>(i));
    }

    freeEntities.push_back(entity);
}

void Registry::clear()
{
    for (auto& component_array : components)
    {
        if (component_array)
        {
            component_array->clear();
        }
    }

    entityMasks.clear();
    for (auto& tag_vec : tags)
    {
        tag_vec.clear();
    }
    freeEntities.clear();
    curEntity = 0;
}

void Registry::addTag(Entity entity, TagName tag)
{
    auto& tag_vec = tags[tagIndex(tag)];

    if (!hasTag(entity, tag))
    {
        tag_vec.push_back(entity);
    }
}

void Registry::removeTag(Entity entity, TagName tag)
{
    auto& tag_vec = tags[tagIndex(tag)];

    for (std::size_t i = 0; i < tag_vec.size(); ++i)
    {
        if (tag_vec[i] == entity)
        {
            tag_vec[i] = tag_vec.back();
            tag_vec.pop_back();
            return;
        }
    }
}

bool Registry::hasTag(Entity entity, TagName tag)
{
    const auto& tag_vec = tags[tagIndex(tag)];

    for (Entity e : tag_vec)
    {
        if (e == entity)
        {
            return true;
        }
    }

    return false;
}

std::vector<Entity>& Registry::getTag(TagName tag)
{
    return tags[tagIndex(tag)];
}
