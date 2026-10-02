#ifndef EQUORUS_ADAPTERS_LINEP_HPP
#define EQUORUS_ADAPTERS_LINEP_HPP

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "equorus/envelope.hpp"
#include "equorus/pilot.hpp"

namespace equorus::adapters::linep {

enum class RuntimeProfile : uint8_t {
    unspecified = 0,
    generate = 1,
    chat = 2,
    embed = 3,
};

struct StreamIdentity {
    uint64_t request_id{0};
    uint64_t execution_id{0};
    uint32_t output_id{0};

    bool is_valid() const noexcept {
        return request_id != 0 && execution_id != 0;
    }
};

struct GenerationOptions {
    float top_p{0.9f};
    int32_t top_k{40};
    float repeat_penalty{1.0f};
    int32_t repeat_last_n{64};
    uint64_t seed{0};
    float presence_penalty{0.0f};
    float frequency_penalty{0.0f};
    std::vector<std::string> stop_sequences;
    std::vector<std::pair<std::string, std::string>> extra_options;
};

struct LinePRequestEnvelope {
    StreamIdentity stream;
    RuntimeProfile profile{RuntimeProfile::generate};
    std::string model_id;
    std::string payload;
    uint32_t max_tokens{0};
    float temperature{0.7f};
    bool stream_requested{true};
    bool has_options{false};
    GenerationOptions options;
};

Envelope to_envelope(
    const LinePRequestEnvelope& req,
    const Value::Object& provenance = {{"kind", Value("SOURCE_LITERAL")}},
    const Limits& limits = {}
);

LinePRequestEnvelope from_envelope(const Envelope& env);

} // namespace equorus::adapters::linep

#endif /* EQUORUS_ADAPTERS_LINEP_HPP */
