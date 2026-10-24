// settings/ai.hpp
#pragma once
#include <string>
#include <string_view>

namespace Default
{
    enum class AIProvider
    {
        Ollama,
        DigitalOcean
    };
    inline constexpr AIProvider AI_PROVIDER = AIProvider::Ollama;
    inline constexpr std::string_view OLLAMA_URL = "http://localhost:11434";
    // Open-weight Qwen 3.5 (3.3 GB): answers without thinking when asked, so tips are quick.
    inline constexpr std::string_view OLLAMA_MODEL = "qwen3.5:4b";
    // DigitalOcean serverless inference: OpenAI-compatible, authenticated by a model access key.
    inline constexpr std::string_view DIGITALOCEAN_URL = "https://inference.do-ai.run";
    // Open-weight (Apache 2.0) model hosted by DigitalOcean.
    inline constexpr std::string_view DIGITALOCEAN_MODEL = "openai-gpt-oss-120b";
    inline constexpr std::string_view DIGITALOCEAN_SECRET = "digitalocean-model-access-key";
    inline constexpr const char* DIGITALOCEAN_KEY_ENVIRONMENT = "DIGITALOCEAN_MODEL_ACCESS_KEY";
    // Where a hovered key's AI tip appears; Off also stops requesting tips on hover.
    enum class TipPlacement
    {
        Off,
        Below,
        Right
    };
    inline constexpr TipPlacement TIP_PLACEMENT = TipPlacement::Below;
} // namespace Default

namespace S
{
    inline Default::AIProvider aiProvider = Default::AI_PROVIDER;
    inline std::string ollamaUrl{Default::OLLAMA_URL};
    inline std::string ollamaModel{Default::OLLAMA_MODEL};
    inline std::string digitalOceanModel{Default::DIGITALOCEAN_MODEL};
    inline Default::TipPlacement tipPlacement = Default::TIP_PLACEMENT;
} // namespace S

inline std::string_view AIProviderName(Default::AIProvider provider)
{
    return provider == Default::AIProvider::DigitalOcean ? "DigitalOcean" : "Ollama";
}

inline bool HoverTipsShown()
{
    return S::tipPlacement != Default::TipPlacement::Off;
}

inline std::string& CurrentAIModel()
{
    return S::aiProvider == Default::AIProvider::DigitalOcean ? S::digitalOceanModel
                                                              : S::ollamaModel;
}

inline std::string_view DefaultAIModel()
{
    return S::aiProvider == Default::AIProvider::DigitalOcean ? Default::DIGITALOCEAN_MODEL
                                                              : Default::OLLAMA_MODEL;
}
