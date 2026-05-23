#include <llvm/Support/FileSystem.h>
#include <llvm/Support/FileUtilities.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/Program.h>

#include <optional>
#include <print>

struct ReadElfInfo {
    std::string cwd;
    std::string moduleName;
    std::vector<std::string> imports;
};

int
main(int argc, char *argv[])
{
    llvm::SmallString<64> OutputFile;
    auto _err = llvm::sys::fs::createTemporaryFile("readref", "", OutputFile);
    llvm::FileRemover OutRemover(OutputFile);
    std::optional<llvm::StringRef> Redirects[3] = {/*Stdin*/ {""}, {OutputFile.str()}, {}};
    std::string errorMessage;
    auto readelf = llvm::sys::findProgramByName("readelf");
    if (!readelf) {
        std::println("Cannot find readref");
        return 0;
    }
    int Ret = llvm::sys::ExecuteAndWait(*readelf,
                                        {"readelf", "-p.gnu.c++.README", "gcm.cache/hellob.gcm"},
                                        std::nullopt,
                                        Redirects,
                                        10,
                                        0,
                                        &errorMessage);
    std::println("ret = {}", Ret);
    if (Ret != 0) {
        std::println("error, {}", errorMessage);
        return 0;
    }
    auto Buf = llvm::MemoryBuffer::getFile(OutputFile);

    if (!Buf) {
        std::println("Can't read readref output: {0}", Buf.getError().message());
        return 0;
    }
    llvm::StringRef Path = Buf->get()->getBuffer().trim();
    if (Path.empty()) {
        return 0;
    }
    std::println("path = {}", Path.str());

    return 0;
}
