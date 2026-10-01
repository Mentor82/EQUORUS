#include "equorus/pilot.hpp"
#include <iostream>
#include <charconv>
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif
int main(int argc,char** argv) {
#ifdef _WIN32
    _setmode(_fileno(stdin),_O_BINARY);
    _setmode(_fileno(stdout),_O_BINARY);
#endif
    if (argc!=2 && argc!=6) return 2;
    equorus::Limits limits;
    if (argc==6) {
        std::size_t* fields[]={&limits.max_bytes,&limits.max_depth,&limits.max_items,&limits.max_string_length};
        for (int i=0;i<4;++i) {
            const std::string_view arg=argv[i+2];
            auto [end,ec]=std::from_chars(arg.data(),arg.data()+arg.size(),*fields[i]);
            if (ec!=std::errc{} || end!=arg.data()+arg.size()) return 2;
        }
    }
    try {
        std::string raw;
        char c;
        while (std::cin.get(c)) {
            if (raw.size()>=limits.max_bytes) equorus::fail(equorus::ErrorCode::limit);
            raw.push_back(c);
        }
        auto envelope=equorus::pilot::decode(raw,argv[1],limits);
        // Deep native snapshot and native encode, not a raw JSON echo.
        auto snapshot=equorus::pilot::create(envelope.value(),argv[1],limits);
        std::cout<<snapshot.encode(equorus::JsonCodec{},limits);
    } catch (const equorus::Error& error) {
        std::cout<<"ERROR "<<error.what();
        return 1;
    } catch (const std::exception& error) {
        std::cerr<<error.what();
        return 2;
    }
}
