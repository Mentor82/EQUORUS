#include "equorus/codec.hpp"
#include "equorus/envelope.hpp"
#include "equorus/pilot.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <random>
#include <string>
#include <vector>

using namespace equorus;

static void require(bool cond, const std::string& msg) {
    if (!cond) {
        std::cerr << "Fuzz/Limits test assertion failed: " << msg << "\n";
        std::exit(1);
    }
}

static void test_depth_limits() {
    JsonCodec codec;

    // Test exact depth boundary
    // Depth 5: [[[[ 1 ]]]] -> depth 1 is outer, 2 is next, 3 is next, 4 is next, 5 is number 1
    std::string valid_depth5 = "[[[[1]]]]";
    auto val = codec.decode(valid_depth5, Limits{65536, 5, 2048, 8192});
    require(std::holds_alternative<Value::Array>(val.data), "valid depth 5 decoded");

    bool threw = false;
    try {
        codec.decode(valid_depth5, Limits{65536, 4, 2048, 8192});
    } catch (const Error& e) {
        threw = (e.code() == ErrorCode::limit);
    }
    require(threw, "depth 5 rejected when max_depth is 4");

    // Extreme depth stress test: 2000 brackets (must reject without stack overflow)
    std::string deep(2000, '[');
    deep.append(2000, ']');
    threw = false;
    try {
        codec.decode(deep, Limits{65536, 12, 2048, 8192});
    } catch (const Error& e) {
        threw = (e.code() == ErrorCode::limit);
    }
    require(threw, "extreme depth rejected safely by limits without stack overflow");
}

static void test_item_limits() {
    JsonCodec codec;

    // Build array with exactly 100 items: [0, 1, 2, ..., 99]
    std::string items100 = "[";
    for (int i = 0; i < 100; ++i) {
        if (i > 0) items100 += ",";
        items100 += std::to_string(i);
    }
    items100 += "]";

    // Root array counts as 1 item, plus 100 elements = 101 items
    auto val = codec.decode(items100, Limits{65536, 12, 101, 8192});
    require(std::get<Value::Array>(val.data).size() == 100, "100 items array decoded");

    bool threw = false;
    try {
        codec.decode(items100, Limits{65536, 12, 100, 8192});
    } catch (const Error& e) {
        threw = (e.code() == ErrorCode::limit);
    }
    require(threw, "101 items rejected when max_items is 100");
}

static void test_string_limits() {
    JsonCodec codec;

    // String with exactly 100 ASCII chars
    std::string s100 = "\"" + std::string(100, 'a') + "\"";
    auto val = codec.decode(s100, Limits{65536, 12, 2048, 100});
    require(std::get<std::string>(val.data).size() == 100, "100 byte string decoded");

    bool threw = false;
    try {
        codec.decode(s100, Limits{65536, 12, 2048, 99});
    } catch (const Error& e) {
        threw = (e.code() == ErrorCode::limit);
    }
    require(threw, "100 byte string rejected when max_string_length is 99");

    // Multi-byte UTF-8 string: 🌙 is 4 bytes (\xF0\x9F\x8C\x99)
    std::string moon = "\"\\ud83c\\udf19\""; // 12 chars escaped, 4 UTF-8 bytes decoded
    auto moon_val = codec.decode(moon, Limits{65536, 12, 2048, 4});
    require(std::get<std::string>(moon_val.data).size() == 4, "moon 4 bytes decoded");

    threw = false;
    try {
        codec.decode(moon, Limits{65536, 12, 2048, 3});
    } catch (const Error& e) {
        threw = (e.code() == ErrorCode::limit);
    }
    require(threw, "4-byte surrogate moon rejected when max_string_length is 3");
}

static void test_fuzz_mutations() {
    JsonCodec codec;
    std::string base = "{\"type_id\":\"vinox.provenance.snapshot\",\"schema_version\":\"0.1\",\"provenance\":{\"kind\":\"SOURCE_LITERAL\",\"source_id\":\"fixture:test\",\"timestamp_ms\":\"12345\"},\"payload\":{}}";

    std::mt19937_64 rng(0xEA08052026ULL);
    std::uniform_int_distribution<size_t> pos_dist(0, base.size());
    std::uniform_int_distribution<int> byte_dist(0, 255);
    std::uniform_int_distribution<int> action_dist(0, 3); // 0: flip, 1: insert, 2: delete, 3: truncate

    int accepted = 0;
    int rejected = 0;

    for (int iter = 0; iter < 1000; ++iter) {
        std::string mutated = base;
        int action = action_dist(rng);
        if (action == 0 && !mutated.empty()) {
            size_t p = pos_dist(rng) % mutated.size();
            mutated[p] = static_cast<char>(byte_dist(rng));
        } else if (action == 1) {
            size_t p = pos_dist(rng) % (mutated.size() + 1);
            mutated.insert(p, 1, static_cast<char>(byte_dist(rng)));
        } else if (action == 2 && !mutated.empty()) {
            size_t p = pos_dist(rng) % mutated.size();
            mutated.erase(p, 1);
        } else if (action == 3) {
            size_t p = pos_dist(rng) % (mutated.size() + 1);
            mutated.resize(p);
        }

        try {
            codec.decode(mutated);
            accepted++;
        } catch (const Error&) {
            rejected++;
        }
    }

    require(rejected > 0, "fuzzing produced rejections");
    std::cout << "Fuzz test: 1000 randomized mutations evaluated (" << accepted << " accepted, " << rejected << " rejected cleanly without crash).\n";
}

int main() {
    std::cout << "Starting production JSON codec limits and fuzzing stress tests...\n";
    test_depth_limits();
    test_item_limits();
    test_string_limits();
    test_fuzz_mutations();
    std::cout << "All JSON codec limits and fuzzing tests passed successfully.\n";
    return 0;
}
