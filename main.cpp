#include <llvm/Support/FileSystem.h>
#include <llvm/Support/FileUtilities.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/Program.h>
#include <llvm/Support/Regex.h>

#include <optional>
#include <print>
#include <string_view>

using namespace std::string_view_literals;

static const llvm::Regex importRegex = llvm::Regex("import: ([^ ]*) ([^ ]*.gcm)");
static const llvm::Regex moduleRegex = llvm::Regex("module: ([^ ]*)");
static const llvm::Regex sourceRegex = llvm::Regex("source: ([^ ]*)");
static const llvm::Regex cwdregex    = llvm::Regex("cwd: ([^ ]*)");

constexpr auto EXAMPLE_ELF = R"(
dump of section '.gnu.c++.README':
  [     0]  GNU C++ primary interface
  [    1a]  compiler: 16.1.1 20260430
  [    34]  version: 16.1
  [    42]  module: hellob
  [    51]  source: hello.cxx
  [    63]  dialect: C++23
  [    72]  cwd: /home/user/cpp/modulecpp
  [    8f]  repository: gcm.cache
  [    a5]  buildtime: 2026/05/24 03:07:29 UTC
  [    c8]  localtime: 2026/05/24 12:07:29 JST
  [    eb]  import: beta beta.gcm
  [   101]  import: alpha alpha.gcm
)"sv;

namespace {
struct ReadElfInfo
{
    std::string source;
    std::string moduleName;
    std::vector<std::string> imports;

    static std::optional<ReadElfInfo> get(llvm::StringRef source);
};

std::optional<ReadElfInfo>
ReadElfInfo::get(llvm::StringRef content)
{
    std::vector<std::string> imports = {};
    std::string source;
    std::string moduleName;
    std::string cwd;
    {
        llvm::StringRef cwd_text = content;
        llvm::SmallVector<llvm::StringRef, 1> matches;
        std::string error;
        if (!cwdregex.match(cwd_text, &matches, &error)) {
            std::println("match failed: {}", error);
            return std::nullopt;
        }
        cwd = matches[1].trim().str();
    }
    {
        llvm::StringRef import_text = content;

        while (!import_text.empty()) {
            llvm::SmallVector<llvm::StringRef, 2> matches;
            std::string error;
            if (!importRegex.match(import_text, &matches, &error)) {
                std::println("match failed: {}", error);
                break;
            }

            auto import_module = matches[1].trim().str();
            imports.push_back(import_module);
            size_t pos  = import_text.find(matches[0]);
            import_text = import_text.drop_front(pos + matches[0].size());
        }
    }

    {
        llvm::StringRef source_text = content;
        llvm::SmallVector<llvm::StringRef, 1> matches;
        std::string error;
        if (!sourceRegex.match(source_text, &matches, &error)) {
            std::println("match failed: {}", error);
            return std::nullopt;
        }
        llvm::StringRef path_ref            = cwd;
        llvm::SmallString<128> current_path = path_ref;
        llvm::sys::path::append(current_path, matches[1].trim());
        source = current_path.str();
    }
    {
        llvm::StringRef module_text = content;
        llvm::SmallVector<llvm::StringRef, 1> matches;
        std::string error;
        if (!moduleRegex.match(module_text, &matches, &error)) {
            std::println("match failed: {}", error);
            return std::nullopt;
        }
        moduleName = matches[1].trim().str();
    }
    return ReadElfInfo{source, moduleName, imports};
}

std::optional<ReadElfInfo>
scan_gcc(llvm::StringRef path)
{
    llvm::SmallString<64> OutputFile;
    auto _err = llvm::sys::fs::createTemporaryFile("readref", "", OutputFile);
    llvm::FileRemover OutRemover(OutputFile);
    std::optional<llvm::StringRef> Redirects[3] = {/*Stdin*/ {""}, {OutputFile.str()}, {}};
    std::string errorMessage;
    auto readelf = llvm::sys::findProgramByName("readelf");
    if (!readelf) {
        std::println("Cannot find readref");
        return std::nullopt;
    }
    int Ret = llvm::sys::ExecuteAndWait(*readelf,
                                        {"readelf", "-p.gnu.c++.README", path},
                                        std::nullopt,
                                        Redirects,
                                        10,
                                        0,
                                        &errorMessage);
    std::println("ret = {}", Ret);
    if (Ret != 0) {
        std::println("error, {}", errorMessage);
        return std::nullopt;
    }
    auto Buf = llvm::MemoryBuffer::getFile(OutputFile);

    if (!Buf) {
        std::println("Can't read readref output: {0}", Buf.getError().message());
        return std::nullopt;
    }
    llvm::StringRef Path = Buf->get()->getBuffer().trim();
    if (Path.empty()) {
        return std::nullopt;
    }
    llvm::StringRef text = Path;
    return ReadElfInfo::get(text);
}
}

int
main(int argc, char *argv[])
{
    auto info = scan_gcc("./gcm.cache/hellob.gcm");
    if (info) {
        auto imports = info->imports;
        for (const auto import_module : imports) {
            std::println("module: {}", import_module);
        }
        std::println("module name: {}", info->moduleName);
        std::println("source path: {}", info->source);
    }
    return 0;
}
