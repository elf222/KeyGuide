// guide/explanations.cpp
#include "guide/explanations.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <ctime>
#include <deque>
#include <memory>
#include <mutex>
#include <set>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <nlohmann/json.hpp>
#include "gameLogic/shortcut_bindings.hpp"
#include "guide/ai_client.hpp"
#include "guide/explanation_parser.hpp"
#include "platform/platform.hpp"
#include "settings/ai.hpp"
#include "settings/app_paths.hpp"
#include "settings/persistence.hpp"

namespace
{
    using Json = nlohmann::json;
    using Clock = std::chrono::steady_clock;
    using Guide::CatalogEntry;
    using Guide::Domain;

    // Small hover batches keep the overlay responsive; bulk batches amortise the prompt. Local
    // reasoning models (deepseek-r1:8b at ~21 tokens/s) think for thousands of tokens per batch.
    constexpr std::size_t HOVER_BATCH = 8, BULK_BATCH = 24;
    constexpr std::size_t LOCAL_HOVER_BATCH = 4, LOCAL_BULK_BATCH = 8;
    constexpr auto HOVER_RETRY_DELAY = std::chrono::seconds(30);
    // A shortcut whose hover reply had no usable tip (missing, or failed grounding).
    constexpr auto UNEXPLAINED_RETRY_DELAY = std::chrono::minutes(2);
    constexpr std::size_t DOMAIN_COUNT = static_cast<std::size_t>(Domain::Count);

    enum class JobKind
    {
        Batch,
        Models,
        Test
    };

    struct Job
    {
        JobKind kind = JobKind::Batch;
        AI::Endpoint endpoint;
        std::vector<CatalogEntry> entries;
        std::shared_ptr<const Guide::AllowedKeys> allowed;
        bool bulk = false;
        std::uint64_t id = 0;
        // Model lists and tests from before a provider change are discarded.
        std::uint64_t providerEpoch = 0;
    };

    struct Result
    {
        JobKind kind = JobKind::Batch;
        std::vector<std::string> keys;
        std::vector<std::pair<std::string, std::string>> accepted;
        std::size_t rejected = 0;
        std::vector<std::string> models;
        std::string error, model, providerName;
        double seconds = 0;
        bool bulk = false, transportFailed = false;
        std::uint64_t id = 0, providerEpoch = 0;
    };

    struct CachedExplanation
    {
        std::string text, model;
    };

    // Keys a queued or running batch has reserved in `pending`.
    struct Reservation
    {
        std::vector<std::string> keys;
        bool bulk = false;
    };

    // Main-thread state.
    std::vector<CatalogEntry> catalog;
    std::shared_ptr<const Guide::AllowedKeys> allowed_keys;
    std::unordered_map<std::string, CachedExplanation> cache;
    std::unordered_set<std::string> pending;
    std::unordered_map<std::uint64_t, Reservation> in_flight;
    std::unordered_map<std::string, Clock::time_point> unexplained_retry_after;
    std::vector<std::string> models;
    Guide::Status status;
    Clock::time_point hover_retry_after{};
    std::uint64_t provider_epoch = 0, next_job_id = 0;
    std::size_t bulk_outstanding = 0;
    std::string last_bulk_error;
    bool initialised = false;

    // Shared with the worker thread.
    std::mutex mutex;
    std::condition_variable wake;
    std::deque<Job> jobs;
    std::vector<Result> results;
    std::atomic<bool> cancel_current{false};
    // The job the worker is running, so cancellation only aborts the intended kind of request.
    bool active = false, active_bulk = false;
    JobKind active_kind = JobKind::Batch;
    std::uint64_t active_id = 0;
    bool stopping = false;
    std::thread worker;

    std::filesystem::path CachePath()
    {
        return AppDataDirectory() / "explanations.json";
    }

    void LoadCache()
    {
        std::string text;
        if (!ReadWholeFile(CachePath(), text))
        {
            return;
        }
        const auto root = Json::parse(text, nullptr, false);
        if (root.is_discarded() || !root.contains("explanations") ||
            !root["explanations"].is_object())
        {
            return;
        }
        for (const auto& [key, value] : root["explanations"].items())
        {
            if (value.is_object() && value.contains("text") && value["text"].is_string())
            {
                const auto model = value.find("model");
                cache[key] = {value["text"].get<std::string>(),
                              model != value.end() && model->is_string()
                                  ? model->get<std::string>()
                                  : std::string()};
            }
        }
    }

    void SaveCache()
    {
        Json explanations = Json::object();
        for (const auto& [key, value] : cache)
        {
            explanations[key] = {{"text", value.text}, {"model", value.model}};
        }
        const Json root = {{"version", 1}, {"explanations", explanations}};
        WriteFileAtomically(CachePath(), root.dump(1) + "\n");
    }

    Result RunJob(const Job& job)
    {
        Result result;
        result.kind = job.kind;
        result.bulk = job.bulk;
        result.id = job.id;
        result.model = job.endpoint.model;
        result.providerName = job.endpoint.providerName;
        result.providerEpoch = job.providerEpoch;
        if (job.kind == JobKind::Models)
        {
            auto list = AI::ListModels(job.endpoint, &cancel_current);
            result.models = std::move(list.models);
            result.error = std::move(list.error);
            return result;
        }
        if (job.kind == JobKind::Test)
        {
            auto list = AI::ListModels(job.endpoint, &cancel_current);
            result.models = std::move(list.models);
            result.error = std::move(list.error);
            if (result.error.empty() && !job.endpoint.model.empty())
            {
                const auto reply = AI::Chat(job.endpoint, "Reply with the single word OK.",
                                            "Connection test", &cancel_current);
                result.error = reply.error;
                result.seconds = reply.seconds;
            }
            return result;
        }
        std::vector<const CatalogEntry*> batch;
        for (const auto& entry : job.entries)
        {
            batch.push_back(&entry);
            result.keys.push_back(entry.cacheKey);
        }
        const auto prompt = Guide::BuildBatchPrompt(batch);
        const auto reply = AI::Chat(job.endpoint, prompt.system, prompt.user, &cancel_current);
        result.seconds = reply.seconds;
        if (!reply.error.empty())
        {
            result.error = reply.error;
            result.transportFailed = reply.transportFailed;
            return result;
        }
        const auto parsed = Guide::ParseBatchReply(reply.content, batch, *job.allowed);
        if (!parsed.error.empty())
        {
            result.error = job.endpoint.providerName + " / " + job.endpoint.model + ": " +
                           parsed.error;
        }
        result.rejected = parsed.rejected;
        for (const auto& explanation : parsed.accepted)
        {
            result.accepted.push_back({batch[explanation.index]->cacheKey, explanation.text});
        }
        return result;
    }

    void WorkerLoop()
    {
        for (;;)
        {
            Job job;
            {
                std::unique_lock lock(mutex);
                wake.wait(lock, [] { return stopping || !jobs.empty(); });
                if (stopping)
                {
                    return;
                }
                job = std::move(jobs.front());
                jobs.pop_front();
                cancel_current.store(false, std::memory_order_release);
                active = true;
                active_bulk = job.bulk;
                active_kind = job.kind;
                active_id = job.id;
            }
            auto result = RunJob(job);
            {
                std::lock_guard lock(mutex);
                active = false;
                results.push_back(std::move(result));
            }
            Platform::WakeMainLoop();
        }
    }

    // Main thread.
    void Enqueue(Job job, bool urgent)
    {
        job.id = ++next_job_id;
        if (job.kind == JobKind::Batch)
        {
            auto& reservation = in_flight[job.id];
            reservation.bulk = job.bulk;
            for (const auto& entry : job.entries)
            {
                reservation.keys.push_back(entry.cacheKey);
            }
        }
        {
            std::lock_guard lock(mutex);
            if (urgent)
            {
                jobs.push_front(std::move(job));
            }
            else
            {
                jobs.push_back(std::move(job));
            }
        }
        wake.notify_one();
    }

    // Main thread. Releases a batch's keys and bulk count; ApplyBatch ignores its result.
    void Retire(std::uint64_t id)
    {
        const auto found = in_flight.find(id);
        if (found == in_flight.end())
        {
            return;
        }
        for (const auto& key : found->second.keys)
        {
            pending.erase(key);
        }
        if (found->second.bulk)
        {
            --bulk_outstanding;
        }
        in_flight.erase(found);
    }

    // Main thread. Removes queued jobs matching `drop`, aborts the running one and discards
    // finished but unapplied batches, all released at once so an immediate restart can
    // reserve the same shortcuts again.
    template <typename Predicate> void CancelJobs(Predicate drop)
    {
        std::lock_guard lock(mutex);
        for (auto job = jobs.begin(); job != jobs.end();)
        {
            if (!drop(job->kind, job->bulk))
            {
                ++job;
                continue;
            }
            Retire(job->id);
            status.testing &= job->kind != JobKind::Test;
            status.loadingModels &= job->kind != JobKind::Models;
            job = jobs.erase(job);
        }
        if (active && drop(active_kind, active_bulk))
        {
            Retire(active_id);
            cancel_current.store(true, std::memory_order_release);
        }
        for (const auto& result : results)
        {
            if (result.kind == JobKind::Batch && drop(result.kind, result.bulk))
            {
                Retire(result.id);
            }
        }
        status.generating = bulk_outstanding > 0;
    }

    void SetMessage(std::string message, bool error)
    {
        status.message = std::move(message);
        status.messageIsError = error;
    }

    // The provider's default model (qwen3.5:4b on Ollama) when offered, else the first chat model.
    std::string PreferredModel(const std::vector<std::string>& available)
    {
        if (std::ranges::find(available, DefaultAIModel()) != available.end())
        {
            return std::string(DefaultAIModel());
        }
        for (const auto& model : available)
        {
            if (model.find("embed") == std::string::npos)
            {
                return model;
            }
        }
        return {};
    }

    std::string MarkdownCell(std::string_view text)
    {
        std::string cell;
        for (const char c : text)
        {
            cell += c == '|' ? "\\|" : std::string(1, c);
        }
        return cell;
    }

    std::string MarkdownKeys(std::string_view keys)
    {
        const auto cell = MarkdownCell(keys);
        return keys.find('`') == std::string_view::npos ? "`" + cell + "`" : "`` " + cell + " ``";
    }

    void ApplyBatch(const Result& result)
    {
        if (!in_flight.contains(result.id))
        {
            // Cancelled by "Stop", a provider change or "Clear Saved Tips"; already released.
            return;
        }
        Retire(result.id);
        for (const auto& [key, text] : result.accepted)
        {
            cache[key] = {text, result.model};
        }
        status.rejected += result.rejected;
        if (!result.accepted.empty())
        {
            SaveCache();
        }
        if (result.bulk)
        {
            status.generated += result.keys.size();
            status.failed += result.keys.size() - result.accepted.size();
        }
        else if (result.error.empty())
        {
            // Hovering asks every tick, so a reply without a usable tip must not be repeated
            // straight away (empty items, omitted ids or grounding rejections). Failed
            // requests back off below for every shortcut instead.
            const auto retry = Clock::now() + UNEXPLAINED_RETRY_DELAY;
            for (const auto& key : result.keys)
            {
                if (!cache.contains(key))
                {
                    unexplained_retry_after[key] = retry;
                }
            }
        }
        if (!result.error.empty())
        {
            if (result.bulk)
            {
                last_bulk_error = result.error;
            }
            SetMessage(result.error, true);
            if (!result.bulk)
            {
                hover_retry_after = Clock::now() + HOVER_RETRY_DELAY;
            }
            else if (result.transportFailed)
            {
                // Connection / key / HTTP problems would fail every remaining batch too. A bad
                // reply (no JSON, out of tokens) only loses this batch.
                Guide::CancelGeneration();
            }
        }
        if (result.bulk && bulk_outstanding == 0 &&
            !(result.transportFailed && !result.error.empty()))
        {
            const auto explained = std::to_string(status.generated - status.failed);
            SetMessage("Explained " + explained + " of " + std::to_string(status.generated) +
                           " shortcuts with " + result.model +
                           (last_bulk_error.empty() ? "" : "; last problem: " + last_bulk_error),
                       status.failed > 0 && status.generated == status.failed);
        }
        status.generating = bulk_outstanding > 0;
    }

    void ApplyModels(const Result& result)
    {
        if (result.providerEpoch != provider_epoch)
        {
            return;
        }
        status.loadingModels = false;
        if (!result.error.empty())
        {
            SetMessage(result.error, true);
            return;
        }
        models = result.models;
        auto& model = CurrentAIModel();
        if (!model.empty() && (models.empty() || std::ranges::find(models, model) != models.end()))
        {
            return;
        }
        // No model chosen yet, or the saved one was removed (e.g. `ollama rm`).
        const auto missing = model;
        model = PreferredModel(models);
        SaveSettings();
        // An error or a running test / generation keeps its message; the menu still shows the
        // new model next to the provider.
        if (!missing.empty() && !model.empty() && !status.messageIsError && !status.generating &&
            !status.testing)
        {
            SetMessage(missing + " is not installed; using " + model, false);
        }
    }

    void ApplyTest(const Result& result)
    {
        if (result.providerEpoch != provider_epoch)
        {
            return;
        }
        status.testing = false;
        if (result.error.empty())
        {
            models = result.models;
        }
        if (!result.error.empty())
        {
            SetMessage(result.error, true);
            return;
        }
        char seconds[32]{};
        std::snprintf(seconds, sizeof(seconds), "%.1f", result.seconds);
        SetMessage(result.model.empty()
                       ? "Connected to " + result.providerName + " (" +
                             std::to_string(result.models.size()) + " models); choose a model"
                       : "Connected to " + result.providerName + ": " + result.model +
                             " replied in " + seconds + " s",
                   false);
    }
} // namespace

namespace Guide
{
    void Initialise()
    {
        if (initialised)
        {
            return;
        }
        initialised = true;
        LoadCache();
        RebuildCatalog();
        {
            std::lock_guard lock(mutex);
            stopping = false;
        }
        worker = std::thread(WorkerLoop);
        RefreshModels();
    }

    void Shutdown()
    {
        if (!initialised)
        {
            return;
        }
        {
            std::lock_guard lock(mutex);
            stopping = true;
            jobs.clear();
            cancel_current.store(true, std::memory_order_release);
        }
        wake.notify_all();
        if (worker.joinable())
        {
            worker.join();
        }
        initialised = false;
    }

    bool Update()
    {
        std::vector<Result> finished;
        {
            std::lock_guard lock(mutex);
            finished.swap(results);
        }
        for (const auto& result : finished)
        {
            switch (result.kind)
            {
                case JobKind::Batch:
                    ApplyBatch(result);
                    break;
                case JobKind::Models:
                    ApplyModels(result);
                    break;
                case JobKind::Test:
                    ApplyTest(result);
                    break;
            }
        }
        return !finished.empty();
    }

    void RebuildCatalog()
    {
        catalog = BuildCatalog();
        allowed_keys = std::make_shared<const AllowedKeys>(BuildAllowedKeys(catalog));
    }

    std::string_view Explanation(HintId id)
    {
        if (id >= catalog.size())
        {
            return {};
        }
        const auto found = cache.find(catalog[id].cacheKey);
        return found == cache.end() ? std::string_view() : std::string_view(found->second.text);
    }

    bool Pending(HintId id)
    {
        return id < catalog.size() && pending.contains(catalog[id].cacheKey);
    }

    void RequestHover(HintId id, const TrackedState& state, bool aerospace_service)
    {
        if (!initialised || !HoverTipsShown() || id >= catalog.size() ||
            Clock::now() < hover_retry_after)
        {
            return;
        }
        const auto now = Clock::now();
        const auto backing_off = [&](const std::string& key)
        {
            const auto found = unexplained_retry_after.find(key);
            return found != unexplained_retry_after.end() && now < found->second;
        };
        const auto& hovered = catalog[id];
        if (cache.contains(hovered.cacheKey) || pending.contains(hovered.cacheKey) ||
            backing_off(hovered.cacheKey))
        {
            return;
        }
        Job job;
        job.endpoint = AI::CurrentEndpoint();
        if (!AI::EndpointProblem(job.endpoint).empty())
        {
            return;
        }
        job.allowed = allowed_keys;
        job.entries.push_back(hovered);
        pending.insert(hovered.cacheKey);
        const auto batch_size = job.endpoint.local ? LOCAL_HOVER_BATCH : HOVER_BATCH;
        for (const auto& entry : catalog)
        {
            if (job.entries.size() >= batch_size)
            {
                break;
            }
            const bool available = entry.domain == Domain::AeroSpace
                                       ? AeroSpaceShortcutAvailable(entry.id, aerospace_service)
                                       : ShortcutAvailable(entry.id, state);
            if (entry.domain != hovered.domain || !available || cache.contains(entry.cacheKey) ||
                pending.contains(entry.cacheKey) || backing_off(entry.cacheKey))
            {
                continue;
            }
            job.entries.push_back(entry);
            pending.insert(entry.cacheKey);
        }
        Enqueue(std::move(job), true);
    }

    void Generate(Domain domain)
    {
        const auto endpoint = AI::CurrentEndpoint();
        if (const auto problem = AI::EndpointProblem(endpoint); !problem.empty())
        {
            SetMessage(problem, true);
            return;
        }
        std::vector<CatalogEntry> todo;
        for (const auto& entry : catalog)
        {
            if (entry.domain == domain && !cache.contains(entry.cacheKey) &&
                !pending.contains(entry.cacheKey))
            {
                pending.insert(entry.cacheKey);
                todo.push_back(entry);
            }
        }
        if (todo.empty())
        {
            SetMessage("Every " + std::string(DomainName(domain)) + " shortcut is explained",
                       false);
            return;
        }
        if (!status.generating)
        {
            last_bulk_error.clear();
            status.generated = 0;
            status.failed = 0;
            status.toGenerate = 0;
        }
        status.toGenerate += todo.size();
        status.generating = true;
        SetMessage("Explaining " + std::string(DomainName(domain)) + " with " + endpoint.model,
                   false);
        const auto batch_size = endpoint.local ? LOCAL_BULK_BATCH : BULK_BATCH;
        for (std::size_t start = 0; start < todo.size(); start += batch_size)
        {
            Job job;
            job.endpoint = endpoint;
            job.bulk = true;
            job.allowed = allowed_keys;
            const auto end = std::min(todo.size(), start + batch_size);
            job.entries.assign(todo.begin() + static_cast<std::ptrdiff_t>(start),
                               todo.begin() + static_cast<std::ptrdiff_t>(end));
            ++bulk_outstanding;
            Enqueue(std::move(job), false);
        }
    }

    void CancelGeneration()
    {
        const bool running = status.generating;
        CancelJobs([](JobKind kind, bool bulk) { return kind == JobKind::Batch && bulk; });
        // After a failed batch, keep its error visible instead.
        if (running && !status.messageIsError)
        {
            SetMessage("Stopped", false);
        }
    }

    void TestConnection()
    {
        Job job;
        job.kind = JobKind::Test;
        job.endpoint = AI::CurrentEndpoint();
        job.providerEpoch = provider_epoch;
        const auto problem = AI::EndpointProblem(job.endpoint);
        if (!problem.empty() && problem != "Choose a model")
        {
            SetMessage(problem, true);
            return;
        }
        status.testing = true;
        SetMessage("Testing " + job.endpoint.providerName + "...", false);
        Enqueue(std::move(job), true);
    }

    void RefreshModels()
    {
        Job job;
        job.kind = JobKind::Models;
        job.endpoint = AI::CurrentEndpoint();
        job.providerEpoch = provider_epoch;
        if (job.endpoint.needsKey && job.endpoint.apiKey.empty())
        {
            SetMessage("Set a DigitalOcean model access key", true);
            return;
        }
        status.loadingModels = true;
        Enqueue(std::move(job), true);
    }

    void ProviderChanged()
    {
        // Queued and running requests still carry the old endpoint and credential.
        CancelJobs([](JobKind, bool) { return true; });
        ++provider_epoch;
        models.clear();
        hover_retry_after = {};
        unexplained_retry_after.clear();
        // A running test or model refresh is aborted and its stale result ignored.
        status.loadingModels = false;
        status.testing = false;
        SetMessage({}, false);
        RefreshModels();
    }

    void ModelChanged()
    {
        hover_retry_after = {};
        unexplained_retry_after.clear();
        SetMessage({}, false);
    }

    const std::vector<std::string>& AvailableModels()
    {
        return models;
    }

    void ClearCache()
    {
        CancelJobs([](JobKind kind, bool) { return kind == JobKind::Batch; });
        cache.clear();
        unexplained_retry_after.clear();
        status.rejected = 0;
        SaveCache();
        SetMessage("Saved explanations cleared", false);
    }

    std::size_t EntryCount(Domain domain)
    {
        std::unordered_set<std::string_view> keys;
        for (const auto& entry : catalog)
        {
            if (entry.domain == domain)
            {
                keys.insert(entry.cacheKey);
            }
        }
        return keys.size();
    }

    std::size_t ExplainedCount(Domain domain)
    {
        std::unordered_set<std::string_view> keys;
        for (const auto& entry : catalog)
        {
            if (entry.domain == domain && cache.contains(entry.cacheKey))
            {
                keys.insert(entry.cacheKey);
            }
        }
        return keys.size();
    }

    Status CurrentStatus()
    {
        return status;
    }

    bool ExportStudyGuide(Domain domain, std::string& path, std::string& error)
    {
        std::vector<std::string> scopes;
        std::unordered_map<std::string, std::vector<const CatalogEntry*>> by_scope;
        std::unordered_set<std::string_view> seen;
        std::set<std::string> used_models;
        for (const auto& entry : catalog)
        {
            if (entry.domain != domain || !seen.insert(entry.cacheKey).second)
            {
                continue;
            }
            if (!by_scope.contains(entry.scope))
            {
                scopes.push_back(entry.scope);
            }
            by_scope[entry.scope].push_back(&entry);
        }
        const auto explained = ExplainedCount(domain);
        char date[32]{};
        const auto now = std::time(nullptr);
        std::strftime(date, sizeof(date), "%Y-%m-%d %H:%M", std::localtime(&now));
        std::string markdown = "# KeyGuide study guide: " + std::string(DomainName(domain)) +
                               "\n\nGenerated " + date + ". " + std::to_string(explained) +
                               " of " + std::to_string(seen.size()) +
                               " shortcuts have an AI tip.\n\n"
                               "Keys and descriptions come from KeyGuide's keymaps. Tips are "
                               "written by an AI model and rejected if they name a shortcut "
                               "that is not in this keymap.\n";
        for (const auto& scope : scopes)
        {
            markdown += "\n## " + (scope.empty() ? std::string("General") : scope) +
                        "\n\n| Keys | What it does | Tip |\n| --- | --- | --- |\n";
            for (const auto* entry : by_scope[scope])
            {
                const auto found = cache.find(entry->cacheKey);
                std::string does = entry->does;
                if (entry->app == "Zed (Vim mode)" || entry->app == "Neovim")
                {
                    does += " (" + entry->app + " only)";
                }
                if (found != cache.end() && !found->second.model.empty())
                {
                    used_models.insert(found->second.model);
                }
                markdown += "| " + MarkdownKeys(entry->keys) + " | " + MarkdownCell(does) +
                            " | " + (found == cache.end() ? "" : MarkdownCell(found->second.text)) +
                            " |\n";
            }
        }
        if (!used_models.empty())
        {
            markdown += "\n---\nTips generated with:";
            for (const auto& model : used_models)
            {
                markdown += " `" + model + "`";
            }
            markdown += "\n";
        }
        const auto file = AppDataDirectory() / "guides" /
                          (std::string(DomainFileName(domain)) + ".md");
        if (!WriteFileAtomically(file, markdown))
        {
            error = "Could not write " + file.string();
            return false;
        }
        path = file.string();
        return true;
    }
} // namespace Guide
