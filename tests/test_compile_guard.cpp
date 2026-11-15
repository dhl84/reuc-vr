// The journal of open pipeline compiles (renderer/compile_guard.h): a compile whose thread crashed is bad at the next start, a compile
// that was only open when the process ended is not.
#include "check.h"
#include "compile_guard.h"
#include <filesystem>
#include <thread>

int main() {
    const auto dir=std::filesystem::temp_directory_path()/"uc_test_compile_guard";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    uc::PipelineKey a{}, b{}, c{};
    a[0]=1; b[0]=2; c[0]=3;
    {
        uc::CompileGuard guard;
        CHECK(guard.open(dir)==0 && guard.new_bad()==0);
        CHECK(!guard.bad(a,false));
        const int first=guard.begin(a,false);   // a compile that ends well
        CHECK(first>=0);
        CHECK(guard.begin(b,false)==-1);         // one open compile for each thread
        guard.end(first);
        guard.crashed();                         // no open compile on this thread: nothing is marked
        // Two threads compile. The first one crashes in its compile; the second one is only open when the process ends.
        std::thread crashes([&] { CHECK(guard.begin(b,true)>=0); guard.crashed(); });
        crashes.join();
        std::thread killed([&] { CHECK(guard.begin(c,false)>=0); });
        killed.join();
        CHECK(!guard.bad(b,true));               // known only after the next start
    }
    {
        uc::CompileGuard guard;
        CHECK(guard.open(dir)==1 && guard.new_bad()==1);
        CHECK(guard.bad(b,true));
        CHECK(!guard.bad(b,false));              // the specialized pipeline of the same key is another compile
        CHECK(!guard.bad(a,false) && !guard.bad(c,false));
    }
    {
        uc::CompileGuard guard;                  // the bad list stays; the journal of the last run was empty
        CHECK(guard.open(dir)==1 && guard.new_bad()==0);
        CHECK(guard.bad(b,true));
    }
    std::filesystem::remove_all(dir);
    return 0;
}
