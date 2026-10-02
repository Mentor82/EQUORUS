#include "equorus/adapters/vinox.hpp"
#include "equorus/adapters/linep.hpp"
#include "equorus/pilot.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
#include <sstream>

using namespace equorus;

static std::string read_file(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) {
        throw std::runtime_error("Could not open file: " + path);
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static void require(bool cond, const std::string& msg) {
    if (!cond) {
        std::cerr << "Assertion failed: " << msg << "\n";
        std::exit(1);
    }
}

int main(int argc, char** argv) {
    std::string fixtures_dir = "tests/fixtures/pilot-v0.1";
    if (argc > 1) {
        fixtures_dir = argv[1];
    }

    std::cout << "Starting C++ consumer adapter tests...\n";

    /* === VINOX ADAPTER TESTS === */
    {
        // 1. Full VINOX metadata
        vinox_provenance_meta meta_full{};
        meta_full.struct_size = sizeof(vinox_provenance_meta);
        meta_full.kind = VINOX_PROVENANCE_TOOL_EVIDENCE;
        meta_full.source_id = "fixture:vinox/tool/α";
        meta_full.timestamp_ms = 18446744073709551615ULL;

        auto env_full = adapters::vinox::to_envelope(meta_full);
        require(env_full.type_id() == "vinox.provenance.snapshot", "vinox full type_id");

        JsonCodec codec;
        std::string json_full = env_full.encode(codec);

        std::string expected_full = read_file(fixtures_dir + "/vinox-full.json");
        auto decoded_golden = pilot::decode(expected_full, "vinox.provenance.snapshot");
        require(env_full.value() == decoded_golden.value(), "vinox full value matches golden fixture");

        // Reverse conversion
        vinox_provenance_meta back_full{};
        std::string storage;
        adapters::vinox::from_envelope(env_full, &back_full, sizeof(back_full), &storage);
        require(back_full.kind == VINOX_PROVENANCE_TOOL_EVIDENCE, "back full kind");
        require(storage == "fixture:vinox/tool/α", "back full source_id storage");
        require(back_full.source_id == storage.c_str(), "back full source_id pointer");
        require(back_full.timestamp_ms == 18446744073709551615ULL, "back full timestamp_ms");
        require(back_full.struct_size == sizeof(vinox_provenance_meta), "back full struct_size");

        // 2. Minimal VINOX metadata
        vinox_provenance_meta meta_min{};
        meta_min.struct_size = VINOX_PROVENANCE_META_MIN_SIZE;
        meta_min.kind = VINOX_PROVENANCE_SOURCE_LITERAL;

        auto env_min = adapters::vinox::to_envelope(meta_min);
        std::string expected_min = read_file(fixtures_dir + "/vinox-minimal.json");
        auto decoded_min_golden = pilot::decode(expected_min, "vinox.provenance.snapshot");
        require(env_min.value() == decoded_min_golden.value(), "vinox min value matches golden fixture");

        vinox_provenance_meta back_min{};
        adapters::vinox::from_envelope(env_min, &back_min, sizeof(back_min));
        require(back_min.kind == VINOX_PROVENANCE_SOURCE_LITERAL, "back min kind");
        require(back_min.source_id == nullptr, "back min source_id null");
        require(back_min.struct_size == VINOX_PROVENANCE_META_MIN_SIZE, "back min struct_size");

        // 3. Null source ID VINOX metadata
        vinox_provenance_meta meta_null_src{};
        meta_null_src.struct_size = sizeof(vinox_provenance_meta);
        meta_null_src.kind = VINOX_PROVENANCE_DERIVED_CONTEXT;
        meta_null_src.source_id = nullptr;
        meta_null_src.timestamp_ms = 0;

        auto env_null_src = adapters::vinox::to_envelope(meta_null_src);
        std::string expected_null_src = read_file(fixtures_dir + "/vinox-null-source.json");
        auto decoded_null_golden = pilot::decode(expected_null_src, "vinox.provenance.snapshot");
        require(env_null_src.value() == decoded_null_golden.value(), "vinox null source matches golden fixture");

        // 4. Edge cases & error rejections
        bool threw = false;
        try {
            vinox_provenance_meta bad_size{};
            bad_size.struct_size = 4;
            adapters::vinox::to_envelope(bad_size);
        } catch (const Error& e) {
            threw = (e.code() == ErrorCode::limit);
        }
        require(threw, "to_envelope rejects struct_size < 8");

        threw = false;
        try {
            vinox_provenance_meta bad_kind{};
            bad_kind.struct_size = sizeof(vinox_provenance_meta);
            bad_kind.kind = 4;
            adapters::vinox::to_envelope(bad_kind);
        } catch (const Error& e) {
            threw = (e.code() == ErrorCode::schema);
        }
        require(threw, "to_envelope rejects kind > 3");

        threw = false;
        try {
            // Struct too small to receive timestamp present in envelope
            vinox_provenance_meta small_target{};
            adapters::vinox::from_envelope(env_full, &small_target, 12);
        } catch (const Error& e) {
            threw = (e.code() == ErrorCode::limit);
        }
        require(threw, "from_envelope rejects target struct too small for present fields");
    }

    /* === LiNeP ADAPTER TESTS === */
    {
        // 1. LiNeP without options
        std::string expected_no_opt = read_file(fixtures_dir + "/linep-no-options.json");
        auto golden_no_opt = pilot::decode(expected_no_opt, "linep.v02.request");

        adapters::linep::LinePRequestEnvelope req_no_opt{};
        req_no_opt.stream.request_id = 9007199254740993ULL;
        req_no_opt.stream.execution_id = 18446744073709551615ULL;
        req_no_opt.stream.output_id = 0;
        req_no_opt.profile = adapters::linep::RuntimeProfile::chat;
        req_no_opt.model_id = "fixture/model";
        req_no_opt.payload = "{\"messages\":[{\"role\":\"user\",\"content\":\"Grüße 🌙\"}]}";
        req_no_opt.max_tokens = 64;
        req_no_opt.temperature = 0.5f;
        req_no_opt.stream_requested = true;
        req_no_opt.has_options = false;

        auto env_no_opt = adapters::linep::to_envelope(
            req_no_opt,
            {{"kind", Value("SOURCE_LITERAL")}, {"source_id", Value("fixture:synthetic")}}
        );
        require(env_no_opt.value() == golden_no_opt.value(), "linep no-options matches golden fixture");

        auto back_no_opt = adapters::linep::from_envelope(env_no_opt);
        require(back_no_opt.stream.request_id == req_no_opt.stream.request_id, "linep back req_id");
        require(back_no_opt.stream.execution_id == req_no_opt.stream.execution_id, "linep back exec_id");
        require(back_no_opt.stream.output_id == req_no_opt.stream.output_id, "linep back out_id");
        require(back_no_opt.profile == adapters::linep::RuntimeProfile::chat, "linep back profile");
        require(back_no_opt.model_id == "fixture/model", "linep back model_id");
        require(back_no_opt.payload == req_no_opt.payload, "linep back payload");
        require(back_no_opt.max_tokens == 64, "linep back max_tokens");
        require(back_no_opt.temperature == 0.5f, "linep back temperature");
        require(back_no_opt.stream_requested == true, "linep back stream_requested");
        require(back_no_opt.has_options == false, "linep back has_options");

        // 2. LiNeP with options
        std::string expected_opt = read_file(fixtures_dir + "/linep-options.json");
        auto golden_opt = pilot::decode(expected_opt, "linep.v02.request");

        adapters::linep::LinePRequestEnvelope req_opt = req_no_opt;
        req_opt.has_options = true;
        req_opt.options.top_p = 0.9f;
        req_opt.options.top_k = 40;
        req_opt.options.repeat_penalty = 1.0f;
        req_opt.options.repeat_last_n = 64;
        req_opt.options.seed = 18446744073709551615ULL;
        req_opt.options.presence_penalty = 0.0f;
        req_opt.options.frequency_penalty = 0.0f;
        req_opt.options.stop_sequences = {"END", "STOP"};
        // Intentionally provide unsorted extra options (z, then a) to verify canonical sorting by UTF-8 bytes
        req_opt.options.extra_options = {{"z", "2"}, {"a", "1"}};

        auto env_opt = adapters::linep::to_envelope(
            req_opt,
            {{"kind", Value("SOURCE_LITERAL")}, {"source_id", Value("fixture:synthetic")}}
        );
        require(env_opt.value() == golden_opt.value(), "linep options matches golden fixture");

        auto back_opt = adapters::linep::from_envelope(env_opt);
        require(back_opt.has_options == true, "linep opt back has_options");
        require(back_opt.options.top_p == 0.9f, "linep opt back top_p");
        require(back_opt.options.seed == 18446744073709551615ULL, "linep opt back seed");
        require(back_opt.options.stop_sequences.size() == 2, "linep opt back stop_sequences size");
        require(back_opt.options.extra_options.size() == 2, "linep opt back extra_options size");
        require(back_opt.options.extra_options[0].first == "a", "linep opt back extra_options sorted a");
        require(back_opt.options.extra_options[1].first == "z", "linep opt back extra_options sorted z");

        // 3. Error rejections
        bool threw = false;
        try {
            auto bad_req = req_opt;
            bad_req.stream.request_id = 0; // Request ID must be nonzero
            adapters::linep::to_envelope(bad_req);
        } catch (const Error& e) {
            threw = (e.code() == ErrorCode::uint64_range);
        }
        require(threw, "to_envelope rejects zero request_id");

        threw = false;
        try {
            auto bad_req = req_opt;
            bad_req.options.extra_options = {{"dup", "1"}, {"dup", "2"}};
            adapters::linep::to_envelope(bad_req);
        } catch (const Error& e) {
            threw = (e.code() == ErrorCode::option_keys);
        }
        require(threw, "to_envelope rejects duplicate extra_option keys");
    }

    std::cout << "All C++ consumer adapter tests passed successfully.\n";
    return 0;
}
