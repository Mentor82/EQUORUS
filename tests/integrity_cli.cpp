#include "equorus/integrity.hpp"
#include "equorus/pilot.hpp"
#include <charconv>
#include <iostream>
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif

int main(int argc, char** argv) {
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif
    if (argc != 3 && argc != 7) return 2;
    equorus::Limits limits;
    if (argc == 7) {
        std::size_t* fields[] = {&limits.max_bytes, &limits.max_depth, &limits.max_items, &limits.max_string_length};
        for (int i = 0; i < 4; ++i) {
            const std::string_view arg = argv[i + 3];
            auto [end, ec] = std::from_chars(arg.data(), arg.data() + arg.size(), *fields[i]);
            if (ec != std::errc{} || end != arg.data() + arg.size()) return 2;
        }
    }
    try {
        std::string raw;
        char c;
        while (std::cin.get(c)) {
            if (raw.size() >= limits.max_bytes) equorus::fail(equorus::ErrorCode::limit);
            raw.push_back(c);
        }
        const std::string_view mode = argv[1];
        const equorus::JsonCodec codec;
        if (mode == "canonical") {
            std::cout << equorus::canonical_bytes(codec.decode(raw, limits), argv[2], limits);
        } else if (mode == "hash") {
            const auto envelope = equorus::pilot::decode(raw, argv[2], limits);
            std::cout << equorus::encode_integrity(equorus::compute_integrity(envelope,
                equorus::canonical_profile, equorus::integrity_algorithm, limits), limits);
        } else if (mode == "record") {
            std::cout << equorus::encode_integrity(equorus::decode_integrity(raw, limits), limits);
        } else if (mode == "verify") {
            const auto request = codec.decode(raw, limits);
            const auto& object = std::get<equorus::Value::Object>(request.data);
            const auto envelope = equorus::pilot::create(object.at("envelope"), argv[2], limits);
            const auto record = equorus::decode_integrity(codec.encode(object.at("integrity"), limits), limits);
            std::cout << (equorus::verify_integrity(envelope, record, limits) ? "true" : "false");
        } else return 2;
    } catch (const equorus::Error& error) {
        std::cout << "ERROR " << error.what();
        return 1;
    } catch (const std::exception& error) {
        std::cerr << error.what();
        return 2;
    }
}
