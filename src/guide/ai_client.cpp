// guide/ai_client.cpp
#include "guide/ai_client.hpp"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <mutex>
#include <optional>
#include <nlohmann/json.hpp>
#include "platform/platform.hpp"
#include "settings/ai.hpp"
#if defined(KEYGUIDE_HAS_CURL)
#include <curl/curl.h>
#endif

namespace
{
    using Json = nlohmann::json;

    struct HttpResponse
    {
        long status = 0;
        std::string body, error;
    };

    constexpr long CONNECT_TIMEOUT_SECONDS = 10;
    constexpr long LIST_TIMEOUT_SECONDS = 20;
    // Local models on a laptop can take minutes for a batch of explanations.
    constexpr long CHAT_TIMEOUT_SECONDS = 600;
    // Reasoning models (deepseek-r1, gpt-oss) spend most of the budget thinking before the JSON.
    constexpr int CHAT_MAX_TOKENS = 8192;
    // Prompt + reasoning + reply must fit; Ollama's default window truncated reasoning models.
    constexpr int OLLAMA_CONTEXT_TOKENS = 16384;
    // Far above any real reply; protects against a misbehaving server.
    constexpr std::size_t MAX_RESPONSE_BYTES = 4 * 1024 * 1024;
    std::optional<std::string> stored_key;

    const std::string& StoredKey()
    {
        if (!stored_key)
        {
            stored_key = Platform::LoadSecret(Default::DIGITALOCEAN_SECRET);
        }
        return *stored_key;
    }

    std::string TrimTrailingSlash(std::string url)
    {
        while (!url.empty() && url.back() == '/')
        {
            url.pop_back();
        }
        return url;
    }

#if defined(KEYGUIDE_HAS_CURL)
    size_t WriteBody(char* data, size_t size, size_t count, void* user)
    {
        auto* body = static_cast<std::string*>(user);
        if (body->size() + size * count > MAX_RESPONSE_BYTES)
        {
            return 0; // Aborts the transfer with CURLE_WRITE_ERROR.
        }
        body->append(data, size * count);
        return size * count;
    }

    int Progress(void* user, curl_off_t, curl_off_t, curl_off_t, curl_off_t)
    {
        const auto* cancel = static_cast<const std::atomic<bool>*>(user);
        return cancel && cancel->load(std::memory_order_acquire) ? 1 : 0;
    }

    HttpResponse Request(const std::string& url, const std::string& api_key,
                         const std::string* body, long timeout_seconds,
                         const std::atomic<bool>* cancel)
    {
        static std::once_flag initialised;
        std::call_once(initialised, [] { curl_global_init(CURL_GLOBAL_DEFAULT); });
        HttpResponse response;
        CURL* curl = curl_easy_init();
        if (!curl)
        {
            response.error = "Could not start an HTTP request";
            return response;
        }
        curl_slist* headers = curl_slist_append(nullptr, "Accept: application/json");
        if (body)
        {
            headers = curl_slist_append(headers, "Content-Type: application/json");
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body->c_str());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body->size()));
        }
        const auto authorization = "Authorization: Bearer " + api_key;
        if (!api_key.empty())
        {
            headers = curl_slist_append(headers, authorization.c_str());
        }
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteBody);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, CONNECT_TIMEOUT_SECONDS);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_seconds);
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "KeyGuide/1.0");
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, Progress);
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, const_cast<std::atomic<bool>*>(cancel));
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
        const auto result = curl_easy_perform(curl);
        if (result == CURLE_ABORTED_BY_CALLBACK)
        {
            response.error = "Cancelled";
        }
        else if (result != CURLE_OK)
        {
            response.error = curl_easy_strerror(result);
        }
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.status);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        return response;
    }
#else
    HttpResponse Request(const std::string&, const std::string&, const std::string*, long,
                         const std::atomic<bool>*)
    {
        return {0, {}, "This build has no HTTP client (libcurl was not found)"};
    }
#endif

    // Prefer the provider's own error message: OpenAI/Ollama {"error":{"message":...}} or
    // {"error":"..."}, DigitalOcean {"id":"Unauthorized","message":"..."}.
    std::string DescribeFailure(const HttpResponse& response, const AI::Endpoint& endpoint)
    {
        if (!response.error.empty())
        {
            return endpoint.providerName + ": " + response.error;
        }
        std::string detail;
        const auto body = Json::parse(response.body, nullptr, false);
        if (!body.is_discarded() && body.is_object() && body.contains("message") &&
            body["message"].is_string())
        {
            detail = body["message"].get<std::string>();
        }
        else if (!body.is_discarded() && body.is_object() && body.contains("error"))
        {
            const auto& error = body["error"];
            if (error.is_string())
            {
                detail = error.get<std::string>();
            }
            else if (error.is_object() && error.contains("message") &&
                     error["message"].is_string())
            {
                detail = error["message"].get<std::string>();
            }
        }
        if (response.status == 401 || response.status == 403)
        {
            detail = "the API key was rejected" + (detail.empty() ? "" : " (" + detail + ")");
        }
        else if (response.status == 404 && detail.empty())
        {
            detail = "not found; check the server address and model name";
        }
        return endpoint.providerName + ": HTTP " + std::to_string(response.status) +
               (detail.empty() ? "" : " - " + detail.substr(0, 160));
    }
} // namespace

namespace AI
{
    bool NetworkAvailable()
    {
#if defined(KEYGUIDE_HAS_CURL)
        return true;
#else
        return false;
#endif
    }

    Endpoint CurrentEndpoint()
    {
        Endpoint endpoint;
        endpoint.providerName = std::string(AIProviderName(S::aiProvider));
        endpoint.model = CurrentAIModel();
        if (S::aiProvider == Default::AIProvider::DigitalOcean)
        {
            endpoint.baseUrl = std::string(Default::DIGITALOCEAN_URL) + "/v1";
            endpoint.needsKey = true;
            const char* environment = std::getenv(Default::DIGITALOCEAN_KEY_ENVIRONMENT);
            endpoint.apiKey = environment && *environment
                                  ? std::string(environment)
                                  : StoredKey();
        }
        else
        {
            endpoint.ollamaUrl = TrimTrailingSlash(S::ollamaUrl);
            endpoint.baseUrl = endpoint.ollamaUrl + "/v1";
            endpoint.local = true;
        }
        return endpoint;
    }

    bool HasStoredKey()
    {
        return !StoredKey().empty();
    }

    void ForgetCachedKey()
    {
        stored_key.reset();
    }

    std::string EndpointProblem(const Endpoint& endpoint)
    {
        if (!NetworkAvailable())
        {
            return "Built without libcurl";
        }
        if (endpoint.needsKey && endpoint.apiKey.empty())
        {
            return "Set a DigitalOcean model access key";
        }
        if (endpoint.model.empty())
        {
            return "Choose a model";
        }
        return {};
    }

    ModelList ListModels(const Endpoint& endpoint, const std::atomic<bool>* cancel)
    {
        ModelList list;
        const auto response = Request(endpoint.baseUrl + "/models", endpoint.apiKey, nullptr,
                                      LIST_TIMEOUT_SECONDS, cancel);
        if (!response.error.empty() || response.status != 200)
        {
            list.error = DescribeFailure(response, endpoint);
            return list;
        }
        const auto body = Json::parse(response.body, nullptr, false);
        if (body.is_discarded() || !body.contains("data") || !body["data"].is_array())
        {
            list.error = endpoint.providerName + ": unexpected /models response";
            return list;
        }
        for (const auto& model : body["data"])
        {
            if (model.is_object() && model.contains("id") && model["id"].is_string())
            {
                list.models.push_back(model["id"].get<std::string>());
            }
        }
        std::sort(list.models.begin(), list.models.end());
        return list;
    }

    ChatReply Chat(const Endpoint& endpoint, std::string_view system, std::string_view user,
                   const std::atomic<bool>* cancel)
    {
        ChatReply reply;
        const auto messages = Json::array(
            {{{"role", "system"}, {"content", system}}, {{"role", "user"}, {"content", user}}});
        const bool ollama = !endpoint.ollamaUrl.empty();
        Json request;
        if (ollama)
        {
            // Ollama's OpenAI-compatible route cannot raise the context window; reasoning models
            // overflowed the default one and their replies were truncated mid-JSON.
            request = {
                {"model", endpoint.model},
                {"messages", messages},
                {"stream", false},
                {"format", "json"},
                {"think", false}, // Honoured by models with switchable thinking.
                {"options",
                 {{"temperature", 0.2},
                  {"num_ctx", OLLAMA_CONTEXT_TOKENS},
                  {"num_predict", CHAT_MAX_TOKENS}}},
            };
        }
        else
        {
            request = {
                {"model", endpoint.model}, {"messages", messages},
                {"temperature", 0.2},      {"max_tokens", CHAT_MAX_TOKENS},
                {"stream", false},
            };
        }
        const auto body = request.dump();
        const auto url =
            ollama ? endpoint.ollamaUrl + "/api/chat" : endpoint.baseUrl + "/chat/completions";
        const auto started = std::chrono::steady_clock::now();
        const auto response = Request(url, endpoint.apiKey, &body, CHAT_TIMEOUT_SECONDS, cancel);
        reply.seconds =
            std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
        if (!response.error.empty() || response.status != 200)
        {
            reply.error = DescribeFailure(response, endpoint);
            reply.transportFailed = true;
            return reply;
        }
        const auto parsed = Json::parse(response.body, nullptr, false);
        // Ollama: {"message":{...},"done_reason":...}; OpenAI: {"choices":[{"message":{...},
        // "finish_reason":...}]}.
        const Json* holder = nullptr;
        if (ollama && parsed.is_object())
        {
            holder = &parsed;
        }
        else if (!ollama && parsed.is_object() && parsed.contains("choices") &&
                 parsed["choices"].is_array() && !parsed["choices"].empty() &&
                 parsed["choices"][0].is_object())
        {
            holder = &parsed["choices"][0];
        }
        if (!holder)
        {
            reply.error = endpoint.providerName + ": unexpected chat response";
            return reply;
        }
        if (holder->contains("message") && (*holder)["message"].is_object())
        {
            const auto& message = (*holder)["message"];
            if (message.contains("content") && message["content"].is_string())
            {
                reply.content = StripModelNoise(message["content"].get<std::string>());
            }
        }
        if (reply.content.empty())
        {
            const char* reason_key = ollama ? "done_reason" : "finish_reason";
            const bool truncated =
                holder->contains(reason_key) && (*holder)[reason_key] == "length";
            reply.error = endpoint.providerName + " / " + endpoint.model +
                          (truncated ? ": ran out of tokens before answering (reasoning models "
                                       "think first; try a smaller or non-reasoning model)"
                                     : ": the model returned no text");
        }
        return reply;
    }

    std::string StripModelNoise(std::string_view content)
    {
        // One pass over the reply: copying while skipping keeps adversarial replies (thousands
        // of fences or think blocks) linear instead of quadratic.
        std::string text;
        text.reserve(content.size());
        for (std::size_t i = 0; i < content.size();)
        {
            if (content.compare(i, 7, "<think>") == 0)
            {
                const auto end = content.find("</think>", i + 7);
                i = end == std::string_view::npos ? content.size() : end + 8;
                continue;
            }
            if (content.compare(i, 3, "```") == 0)
            {
                // Drop the fence and an optional language tag such as ```json.
                i += 3;
                while (i < content.size() && content[i] != '\n' && content[i] != '{' &&
                       content[i] != '[' && content.compare(i, 3, "```") != 0)
                {
                    ++i;
                }
                continue;
            }
            text += content[i++];
        }
        const auto first = text.find_first_not_of(" \t\r\n");
        const auto last = text.find_last_not_of(" \t\r\n");
        return first == std::string::npos ? std::string() : text.substr(first, last - first + 1);
    }
} // namespace AI
