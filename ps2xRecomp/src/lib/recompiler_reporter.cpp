#include "ps2recomp/recompiler_reporter.h"

#include <algorithm>
#include <iomanip>
#include <fstream>
#include <iostream>
#include <sstream>

namespace ps2recomp
{
    namespace
    {
        const char *severityName(RecompilerReporter::Severity severity)
        {
            switch (severity)
            {
            case RecompilerReporter::Severity::Info:
                return "info";
            case RecompilerReporter::Severity::Warning:
                return "warning";
            case RecompilerReporter::Severity::Error:
                return "error";
            }
            return "unknown";
        }

        std::string hexAddress(uint32_t address)
        {
            std::ostringstream ss;
            ss << "0x" << std::hex << address;
            return ss.str();
        }

        std::string jsonEscape(const std::string &value)
        {
            std::ostringstream out;
            for (unsigned char c : value)
            {
                switch (c)
                {
                case '"': out << "\\\""; break;
                case '\\': out << "\\\\"; break;
                case '\b': out << "\\b"; break;
                case '\f': out << "\\f"; break;
                case '\n': out << "\\n"; break;
                case '\r': out << "\\r"; break;
                case '\t': out << "\\t"; break;
                default:
                    if (c < 0x20)
                    {
                        const char *digits = "0123456789abcdef";
                        out << "\\u00" << digits[(c >> 4) & 0xF] << digits[c & 0xF];
                    }
                    else out << static_cast<char>(c);
                }
            }
            return out.str();
        }
    }

    void RecompilerReporter::progress(const std::string &message)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::cout << "[recompiler] " << message << std::endl;
    }

    void RecompilerReporter::info(const std::string &category, const std::string &message)
    {
        addEvent(Severity::Info, category, message);
    }

    void RecompilerReporter::warning(const std::string &category, const std::string &message)
    {
        addEvent(Severity::Warning, category, message);
    }

    void RecompilerReporter::error(const std::string &category, const std::string &message)
    {
        addEvent(Severity::Error, category, message);
    }

    void RecompilerReporter::warningAt(const std::string &category, const std::string &functionName, uint32_t address, const std::string &message)
    {
        addEvent(Severity::Warning, category, message, functionName, address, true);
    }

    void RecompilerReporter::errorAt(const std::string &category, const std::string &functionName, uint32_t address, const std::string &message)
    {
        addEvent(Severity::Error, category, message, functionName, address, true);
    }

    void RecompilerReporter::recordDiscovered(size_t functions, size_t symbols, size_t sections, size_t relocations)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_counters.functionsDiscovered = functions;
        m_counters.symbolsDiscovered = symbols;
        m_counters.sectionsDiscovered = sections;
        m_counters.relocationsDiscovered = relocations;
    }

    void RecompilerReporter::recordFunctionProcessed()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        ++m_counters.functionsProcessed;
    }

    void RecompilerReporter::recordFunctionResult(const std::string &name, uint32_t start, uint32_t end,
                                                  const std::string &status, const std::string &reason)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = std::find_if(m_functionResults.begin(), m_functionResults.end(),
                               [start](const FunctionResult &result) { return result.start == start; });
        FunctionResult result{name, start, end, status, reason};
        if (it == m_functionResults.end()) m_functionResults.push_back(std::move(result));
        else *it = std::move(result);
    }

    bool RecompilerReporter::writeJson(const std::string &path) const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        if (!out) return false;
        out << "{\n  \"schema_version\": 1,\n  \"counters\": {\n";
        out << "    \"functions_discovered\": " << m_counters.functionsDiscovered << ",\n";
        out << "    \"symbols_discovered\": " << m_counters.symbolsDiscovered << ",\n";
        out << "    \"sections_discovered\": " << m_counters.sectionsDiscovered << ",\n";
        out << "    \"relocations_discovered\": " << m_counters.relocationsDiscovered << ",\n";
        out << "    \"functions_processed\": " << m_counters.functionsProcessed << ",\n";
        out << "    \"functions_recompiled\": " << m_counters.functionsRecompiled << ",\n";
        out << "    \"functions_stubbed\": " << m_counters.functionsStubbed << ",\n";
        out << "    \"functions_skipped\": " << m_counters.functionsSkipped << ",\n";
        out << "    \"decode_failures\": " << m_counters.decodeFailures << ",\n";
        out << "    \"additional_entrypoints\": " << m_counters.additionalEntryPoints << ",\n";
        out << "    \"generated_functions\": " << m_counters.generatedFunctions << ",\n";
        out << "    \"unhandled_instructions\": " << m_counters.unhandledInstructions << ",\n";
        out << "    \"indirect_fallback_promotions\": " << m_counters.indirectFallbackPromotions << ",\n";
        out << "    \"indirect_fallback_entries\": " << m_counters.indirectFallbackEntries << ",\n";
        out << "    \"correctness_critical_guest_fallbacks\": " << m_counters.correctnessCriticalGuestFallbacks << ",\n";
        out << "    \"correctness_critical_failures\": " << m_counters.correctnessCriticalFailures << "\n  },\n";
        out << "  \"functions\": [";
        for (size_t i = 0; i < m_functionResults.size(); ++i)
        {
            const auto &f = m_functionResults[i];
            out << (i ? ",\n" : "\n") << "    {\"name\": \"" << jsonEscape(f.name)
                << "\", \"start\": " << f.start << ", \"start_hex\": \"" << hexAddress(f.start)
                << "\", \"end\": " << f.end << ", \"end_hex\": \"" << hexAddress(f.end)
                << "\", \"status\": \"" << jsonEscape(f.status)
                << "\", \"reason\": \"" << jsonEscape(f.reason) << "\"}";
        }
        if (!m_functionResults.empty()) out << "\n  ";
        out << "],\n  \"events\": [";
        for (size_t i = 0; i < m_events.size(); ++i)
        {
            const auto &e = m_events[i];
            out << (i ? ",\n" : "\n") << "    {\"severity\": \"" << severityName(e.severity)
                << "\", \"category\": \"" << jsonEscape(e.category)
                << "\", \"function\": \"" << jsonEscape(e.functionName)
                << "\", \"has_address\": " << (e.hasAddress ? "true" : "false")
                << ", \"address\": " << (e.hasAddress ? std::to_string(e.address) : "null")
                << ", \"message\": \"" << jsonEscape(e.message) << "\"}";
        }
        if (!m_events.empty()) out << "\n  ";
        out << "]\n}\n";
        return static_cast<bool>(out);
    }

    void RecompilerReporter::recordFunctionRecompiled()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        ++m_counters.functionsRecompiled;
    }

    void RecompilerReporter::recordFunctionStubbed()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        ++m_counters.functionsStubbed;
    }

    void RecompilerReporter::recordFunctionSkipped()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        ++m_counters.functionsSkipped;
    }

    void RecompilerReporter::recordDecodeFailure()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        ++m_counters.decodeFailures;
    }

    void RecompilerReporter::recordAdditionalEntryPoints(size_t count)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_counters.additionalEntryPoints += count;
    }

    void RecompilerReporter::recordGeneratedFunctions(size_t count)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_counters.generatedFunctions += count;
    }

    void RecompilerReporter::recordCorrectnessCriticalGuestFallback()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        ++m_counters.correctnessCriticalGuestFallbacks;
    }

    void RecompilerReporter::recordCorrectnessCriticalFailure()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        ++m_counters.correctnessCriticalFailures;
    }

    void RecompilerReporter::recordIndirectFallbackPromotion(const std::string &functionName,
                                                             const std::vector<uint32_t> &jumpAddresses,
                                                             size_t promotedEntryCount)
    {
        std::ostringstream ss;
        ss << "unresolved JR/JALR at";
        for (uint32_t address : jumpAddresses)
        {
            ss << ' ' << hexAddress(address);
        }
        ss << "; promoted " << promotedEntryCount << " fallback entr" << (promotedEntryCount == 1 ? "y" : "ies");

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            ++m_counters.indirectFallbackPromotions;
            m_counters.indirectFallbackEntries += promotedEntryCount;
            m_events.push_back(Event{Severity::Warning, "control-flow", ss.str(), functionName, jumpAddresses.empty() ? 0u : jumpAddresses.front(), !jumpAddresses.empty()});
        }
    }

    void RecompilerReporter::recordUnhandledInstruction(const std::string &functionName,
                                                        uint32_t address,
                                                        uint32_t raw,
                                                        const std::string &message)
    {
        std::ostringstream ss;
        ss << message << " raw=0x" << std::hex << raw;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            ++m_counters.unhandledInstructions;
            m_events.push_back(Event{Severity::Error, "unhandled-instruction", ss.str(), functionName, address, true});
        }
    }

    const RecompilerReporter::Counters &RecompilerReporter::counters() const
    {
        return m_counters;
    }

    bool RecompilerReporter::hasErrors() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return std::any_of(m_events.begin(), m_events.end(), [](const Event &event) { return event.severity == Severity::Error; });
    }

    bool RecompilerReporter::hasWarnings() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return std::any_of(m_events.begin(), m_events.end(), [](const Event &event) { return event.severity == Severity::Warning; });
    }

    void RecompilerReporter::printSummary(std::ostream &os) const
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        os << "\n========== PS2Recomp report ==========" << std::endl;
        os << "Functions discovered: " << m_counters.functionsDiscovered << std::endl;
        os << "Symbols: " << m_counters.symbolsDiscovered
           << ", sections: " << m_counters.sectionsDiscovered
           << ", relocations: " << m_counters.relocationsDiscovered << std::endl;
        os << "Functions processed: " << m_counters.functionsProcessed
           << ", recompiled: " << m_counters.functionsRecompiled
           << ", stubs: " << m_counters.functionsStubbed
           << ", skipped: " << m_counters.functionsSkipped
           << ", decode failures: " << m_counters.decodeFailures << std::endl;
        os << "Additional entrypoints: " << m_counters.additionalEntryPoints << std::endl;
        os << "Generated functions: " << m_counters.generatedFunctions << std::endl;
        os << "Indirect fallback promotions: " << m_counters.indirectFallbackPromotions
           << " (" << m_counters.indirectFallbackEntries << " fallback entries)" << std::endl;
        os << "Unhandled instructions: " << m_counters.unhandledInstructions << std::endl;
        os << "Correctness-critical guest fallbacks: " << m_counters.correctnessCriticalGuestFallbacks
           << ", failures: " << m_counters.correctnessCriticalFailures << std::endl;

        size_t warnings = 0;
        size_t errors = 0;
        for (const Event &event : m_events)
        {
            warnings += event.severity == Severity::Warning ? 1u : 0u;
            errors += event.severity == Severity::Error ? 1u : 0u;
        }
        os << "Warnings: " << warnings << ", errors: " << errors << std::endl;

        if (!m_events.empty())
        {
            os << "\nEvents:" << std::endl;
            const size_t count =  m_events.size();
            for (size_t i = 0; i < count; ++i)
            {
                const Event &event = m_events[i];
                os << "  [" << severityName(event.severity) << "] " << event.category;
                if (!event.functionName.empty())
                {
                    os << " function=" << event.functionName;
                }
                if (event.hasAddress)
                {
                    os << " addr=" << hexAddress(event.address);
                }
                os << " - " << event.message << std::endl;
            }
        }

        os << "======================================" << std::endl;
    }

    void RecompilerReporter::addEvent(Severity severity,
                                      const std::string &category,
                                      const std::string &message,
                                      const std::string &functionName,
                                      uint32_t address,
                                      bool hasAddress)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_events.push_back(Event{severity, category, message, functionName, address, hasAddress});
    }
}
