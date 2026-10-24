// guide/command_line.cpp
#include "guide/command_line.hpp"
#include <chrono>
#include <cstdio>
#include <string>
#include <thread>
#include "guide/explanations.hpp"
#include "keymaps/keymap_files.hpp"
#include "settings/ai.hpp"

namespace
{
    constexpr auto POLL_INTERVAL = std::chrono::milliseconds(100);
    constexpr auto MODEL_LIST_TIMEOUT = std::chrono::seconds(30);

    bool ParseDomain(std::string_view name, Guide::Domain& domain)
    {
        for (int i = 0; i < static_cast<int>(Guide::Domain::Count); ++i)
        {
            const auto candidate = static_cast<Guide::Domain>(i);
            if (Guide::DomainFileName(candidate) == name)
            {
                domain = candidate;
                return true;
            }
        }
        return false;
    }
} // namespace

int RunExplainCommand(std::string_view name)
{
    Guide::Domain domain{};
    if (!ParseDomain(name, domain))
    {
        std::fprintf(stderr, "usage: KeyGuide --explain vim|blender|aerospace\n");
        return 2;
    }
    LoadKeymapFiles();
    Guide::Initialise();
    // An empty model setting defaults to the provider's first model once the list arrives.
    const auto deadline = std::chrono::steady_clock::now() + MODEL_LIST_TIMEOUT;
    while (Guide::CurrentStatus().loadingModels && std::chrono::steady_clock::now() < deadline)
    {
        Guide::Update();
        std::this_thread::sleep_for(POLL_INTERVAL);
    }
    std::printf("%s · %s · %s\n", std::string(AIProviderName(S::aiProvider)).c_str(),
                CurrentAIModel().empty() ? "(no model)" : CurrentAIModel().c_str(),
                std::string(Guide::DomainName(domain)).c_str());
    Guide::Generate(domain);
    std::size_t reported = static_cast<std::size_t>(-1);
    while (Guide::CurrentStatus().generating)
    {
        Guide::Update();
        const auto status = Guide::CurrentStatus();
        if (status.generated != reported)
        {
            reported = status.generated;
            std::printf("  %zu / %zu\n", status.generated, status.toGenerate);
            std::fflush(stdout);
        }
        std::this_thread::sleep_for(POLL_INTERVAL);
    }
    Guide::Update();
    const auto status = Guide::CurrentStatus();
    std::printf("%s\n", status.message.c_str());
    std::printf("%zu / %zu shortcuts explained, %zu tips rejected by the grounding check\n",
                Guide::ExplainedCount(domain), Guide::EntryCount(domain), status.rejected);
    std::string path, error;
    if (Guide::ExportStudyGuide(domain, path, error))
    {
        std::printf("Study guide: %s\n", path.c_str());
    }
    else
    {
        std::fprintf(stderr, "%s\n", error.c_str());
    }
    Guide::Shutdown();
    return status.messageIsError || !error.empty() ? 1 : 0;
}
