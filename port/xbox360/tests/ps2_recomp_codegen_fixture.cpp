#include "ps2_ee_test_program.h"

#include "ps2recomp/ps2_recompiler.h"

#include <elfio/elfio.hpp>

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace
{
constexpr std::uint32_t kGuestStart = 0x00100000u;

constexpr std::array<std::uint32_t, 23> makeProbeWords()
{
    using namespace ps2::xbox360::ee::test_program::detail;
    return {
        kProgramWords[0],                         // LUI r1,0x1234
        kProgramWords[1],                         // ORI r1,r1,0x5678
        kProgramWords[2],                         // ADDIU r2,r1,-1
        kProgramWords[3],                         // XORI r3,r2,0x00ff
        iType(0x09u, 3u, 4u, 0x0000u),            // ADDIU r4,r3,0
        rType(3u, 4u, 5u, 0x21u),                 // ADDU r5,r3,r4
        iType(0x04u, 3u, 4u, 0x0002u),            // BEQ r3,r4,+2
        iType(0x09u, 0u, 6u, 0x0009u),            // delay slot: ADDIU r6,r0,9
        iType(0x09u, 0u, 7u, 0x0063u),            // skipped
        iType(0x09u, 6u, 7u, 0x0003u),            // target: ADDIU r7,r6,3
        iType(0x2Bu, 8u, 9u, 0x0000u),            // SW r9,0(r8)
        iType(0x23u, 8u, 10u, 0x0000u),           // LW r10,0(r8)
        iType(0x24u, 8u, 11u, 0x0001u),           // LBU r11,1(r8)
        iType(0x20u, 8u, 12u, 0x0003u),           // LB r12,3(r8)
        iType(0x25u, 8u, 13u, 0x0002u),           // LHU r13,2(r8)
        iType(0x3Fu, 8u, 14u, 0x0008u),           // SD r14,8(r8)
        iType(0x37u, 8u, 15u, 0x0008u),           // LD r15,8(r8)
        iType(0x28u, 8u, 16u, 0x000Fu),           // SB r16,15(r8)
        iType(0x24u, 8u, 17u, 0x000Fu),           // LBU r17,15(r8)
        iType(0x29u, 8u, 18u, 0x0010u),           // SH r18,16(r8)
        iType(0x25u, 8u, 19u, 0x0010u),           // LHU r19,16(r8)
        rType(31u, 0u, 0u, 0x08u),                // JR r31
        0u                                         // delay slot: NOP
    };
}

bool writeFixtureElf(const std::filesystem::path &path)
{
    ELFIO::elfio writer;
    writer.create(ELFIO::ELFCLASS32, ELFIO::ELFDATA2LSB);
    writer.set_os_abi(ELFIO::ELFOSABI_NONE);
    writer.set_type(ELFIO::ET_EXEC);
    writer.set_machine(ELFIO::EM_MIPS);
    writer.set_entry(kGuestStart);

    constexpr auto words = makeProbeWords();
    std::array<char, words.size() * sizeof(std::uint32_t)> textBytes{};
    for (std::size_t i = 0; i < words.size(); ++i)
    {
        for (unsigned byte = 0; byte < 4u; ++byte)
        {
            textBytes[i * 4u + byte] =
                static_cast<char>((words[i] >> (byte * 8u)) & 0xFFu);
        }
    }

    ELFIO::section *text = writer.sections.add(".text");
    text->set_type(ELFIO::SHT_PROGBITS);
    text->set_flags(ELFIO::SHF_ALLOC | ELFIO::SHF_EXECINSTR);
    text->set_addr_align(4u);
    text->set_address(kGuestStart);
    text->set_data(textBytes.data(), static_cast<ELFIO::Elf_Word>(textBytes.size()));

    ELFIO::section *strtab = writer.sections.add(".strtab");
    strtab->set_type(ELFIO::SHT_STRTAB);
    strtab->set_addr_align(1u);

    ELFIO::section *symtab = writer.sections.add(".symtab");
    symtab->set_type(ELFIO::SHT_SYMTAB);
    symtab->set_info(1u);
    symtab->set_link(strtab->get_index());
    symtab->set_addr_align(4u);
    symtab->set_entry_size(writer.get_default_entry_size(ELFIO::SHT_SYMTAB));

    ELFIO::symbol_section_accessor symbols(writer, symtab);
    ELFIO::string_section_accessor strings(strtab);
    symbols.add_symbol(strings, "", 0u, 0u, ELFIO::STB_LOCAL,
                       ELFIO::STT_NOTYPE, 0u, ELFIO::SHN_UNDEF);
    symbols.add_symbol(strings, "mksm_ee_codegen_probe", kGuestStart,
                       static_cast<ELFIO::Elf_Word>(textBytes.size()),
                       ELFIO::STB_GLOBAL, ELFIO::STT_FUNC, 0u, text->get_index());

    ELFIO::segment *segment = writer.segments.add();
    segment->set_type(ELFIO::PT_LOAD);
    segment->set_flags(ELFIO::PF_R | ELFIO::PF_X);
    segment->set_align(0x1000u);
    segment->add_section_index(text->get_index(), text->get_addr_align());

    return writer.save(path.string());
}

bool writeConfig(const std::filesystem::path &path,
                 const std::filesystem::path &elfPath,
                 const std::filesystem::path &outputPath)
{
    std::ofstream config(path);
    if (!config)
    {
        return false;
    }

    config << "[general]\n";
    config << "input = " << std::quoted(elfPath.generic_string()) << "\n";
    config << "output = " << std::quoted(outputPath.generic_string()) << "\n";
    config << "single_file_output = true\n";
    config << "low_memory_mode = true\n";
    config << "output_worker_threads = 1\n";
    return static_cast<bool>(config);
}

bool containsAll(const std::string &source, const std::vector<std::string> &needles)
{
    for (const auto &needle : needles)
    {
        if (source.find(needle) == std::string::npos)
        {
            std::cerr << "Generated C++ is missing expected fragment: " << needle << '\n';
            return false;
        }
    }
    return true;
}
} // namespace

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        std::cerr << "Usage: mksm_xbox360_codegen_fixture <output-directory>\n";
        return 2;
    }

    const std::filesystem::path root = std::filesystem::absolute(argv[1]);
    const std::filesystem::path generatedDir = root / "generated";
    std::error_code error;
    std::filesystem::create_directories(generatedDir, error);
    if (error)
    {
        std::cerr << "Could not create probe output directory: " << error.message() << '\n';
        return 1;
    }

    const auto elfPath = root / "mksm_ee_codegen_probe.elf";
    const auto configPath = root / "mksm_ee_codegen_probe.toml";
    if (!writeFixtureElf(elfPath) || !writeConfig(configPath, elfPath, generatedDir))
    {
        std::cerr << "Could not write synthetic PS2 ELF/config fixture\n";
        return 1;
    }

    ps2recomp::PS2Recompiler recompiler(configPath.string());
    if (!recompiler.initialize())
    {
        std::cerr << "PS2Recomp failed to initialize the synthetic fixture\n";
        recompiler.printReport();
        return 1;
    }
    if (!recompiler.recompile())
    {
        std::cerr << "PS2Recomp failed to decode/recompile the synthetic fixture\n";
        recompiler.printReport();
        return 1;
    }

    recompiler.generateOutput();
    const auto generatedCpp = generatedDir / "ps2_recompiled_functions.cpp";
    std::ifstream input(generatedCpp);
    if (!input)
    {
        std::cerr << "PS2Recomp did not produce ps2_recompiled_functions.cpp\n";
        recompiler.printReport();
        return 1;
    }
    const std::string source{
        std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};

    if (!containsAll(source, {
            "mksm_ee_codegen_probe_0x100000",
            "SET_GPR_S32(ctx, 1",
            "SET_GPR_U64(ctx, 1",
            "SET_GPR_S32(ctx, 2",
            "SET_GPR_U64(ctx, 3",
            "branch_taken_0x100018",
            "goto label_100024",
            "WRITE32(",
            "READ32(",
            "READ16(",
            "READ8(",
            "WRITE64(",
            "READ64(",
            "WRITE8(",
            "WRITE16("
        }))
    {
        recompiler.printReport();
        return 1;
    }

    std::cout << "MKSM_RECOMP_GENERATED_CPP instructions=23 entry=00100000 output="
              << generatedCpp.string() << '\n';
    return 0;
}
