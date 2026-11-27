#include "ps2recomp/recompiler_reporter.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

namespace
{
    bool require(bool condition, const char *message)
    {
        if (!condition)
        {
            std::cerr << "REPORTER_TEST_FAIL: " << message << '\n';
            return false;
        }
        return true;
    }

    std::size_t countOccurrences(const std::string &text, const std::string &needle)
    {
        std::size_t count = 0;
        std::size_t offset = 0;
        while ((offset = text.find(needle, offset)) != std::string::npos)
        {
            ++count;
            offset += needle.size();
        }
        return count;
    }
}

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        std::cerr << "usage: ps2recomp_reporter_json_test <output.json>\n";
        return 2;
    }

    namespace fs = std::filesystem;
    const fs::path outputPath(argv[1]);
    fs::create_directories(outputPath.parent_path());

    ps2recomp::RecompilerReporter reporter;
    reporter.recordDiscovered(3, 5, 7, 11);
    reporter.recordFunctionProcessed();
    reporter.recordFunctionProcessed();
    reporter.recordFunctionRecompiled();
    reporter.recordFunctionStubbed();
    reporter.recordDecodeFailure();

    reporter.recordFunctionResult("old name", 0x1000u, 0x1010u, "decode-failed", "old result");
    reporter.recordFunctionResult("updated \"function\"", 0x1000u, 0x1020u, "decoded",
                                  "quote: \" slash: \\ newline:\n tab:\t control:\x01");
    reporter.recordFunctionResult("stub", 0x2000u, 0x2004u, "stubbed", "configured stub");

    reporter.warningAt("unit-test", "updated \"function\"", 0x1004u,
                       "quote: \" slash: \\ newline:\n tab:\t control:\x02");
    reporter.info("unit-test", "event without guest address");

    if (!require(reporter.writeJson(outputPath.string()), "writeJson should create the report"))
    {
        return 1;
    }

    std::ifstream input(outputPath, std::ios::binary);
    const std::string json((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    if (!require(static_cast<bool>(input) || input.eof(), "report should be readable") ||
        !require(json.find("\"schema_version\": 1") != std::string::npos, "schema version missing") ||
        !require(json.find("\"functions_discovered\": 3") != std::string::npos, "counter serialization missing") ||
        !require(json.find(R"("name": "updated \"function\"")") != std::string::npos, "quoted function name was not escaped") ||
        !require(json.find(R"("reason": "quote: \" slash: \\ newline:\n tab:\t control:\u0001")") != std::string::npos,
                 "function reason escaping is incorrect") ||
        !require(json.find(R"("message": "quote: \" slash: \\ newline:\n tab:\t control:\u0002")") != std::string::npos,
                 "event message escaping is incorrect") ||
        !require(json.find(R"("address": null)") != std::string::npos, "address-less event must serialize null") ||
        !require(json.find(R"("has_address": false)") != std::string::npos, "address-less event flag missing") ||
        !require(countOccurrences(json, R"("start": 4096)") == 1u, "function result upsert duplicated an address"))
    {
        return 1;
    }

    const fs::path impossiblePath = outputPath.parent_path() / "missing-parent" / "cannot-write.json";
    fs::remove_all(impossiblePath.parent_path());
    if (!require(!reporter.writeJson(impossiblePath.string()), "writeJson must report an open failure"))
    {
        return 1;
    }

    std::cout << "PS2RECOMP_REPORTER_JSON_PASS functions=2 events=2 escaping=pass upsert=pass\n";
    return 0;
}
