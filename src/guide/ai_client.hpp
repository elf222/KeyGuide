// guide/ai_client.hpp
#pragma once
#include <atomic>
#include <string>
#include <string_view>
#include <vector>

// OpenAI-compatible chat client. Ollama (`<host>/v1`) and DigitalOcean serverless inference
// (`https://inference.do-ai.run/v1`) expose the same /models and /chat/completions routes.
namespace AI
{
    struct Endpoint
    {
        std::string providerName, baseUrl, apiKey, model;
        bool needsKey = false;
        // Set for Ollama: chat uses its native /api/chat route with JSON output and a larger
        // context window. Laptop-sized (often reasoning) models also get smaller batches.
        std::string ollamaUrl;
        bool local = false;
    };

    struct ModelList
    {
        std::vector<std::string> models;
        std::string error;
    };

    struct ChatReply
    {
        std::string content, error;
        double seconds = 0;
        // The request itself failed (connection, HTTP status, key); retrying would fail too.
        bool transportFailed = false;
    };

    // Snapshot of the selected provider; call on the main thread (reads settings and secrets).
    Endpoint CurrentEndpoint();
    // Empty when usable; otherwise a short reason such as "Choose a model".
    std::string EndpointProblem(const Endpoint& endpoint);
    // The stored DigitalOcean key is read from the Keychain once and cached (main thread).
    bool HasStoredKey();
    void ForgetCachedKey();
    bool NetworkAvailable();

    // Blocking; run on a worker thread. `cancel` aborts an in-flight request.
    ModelList ListModels(const Endpoint& endpoint, const std::atomic<bool>* cancel = nullptr);
    ChatReply Chat(const Endpoint& endpoint, std::string_view system, std::string_view user,
                   const std::atomic<bool>* cancel = nullptr);

    // Removes <think>...</think> blocks and Markdown code fences that reasoning / chat models add.
    std::string StripModelNoise(std::string_view content);
} // namespace AI
