#include "equorus/pilot.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <regex>
#include <limits>

namespace equorus::pilot {
namespace {
using Object=Value::Object;
const Object& object(const Value& v) {
    auto p=std::get_if<Object>(&v.data);
    if (!p) fail(ErrorCode::schema);
    return *p;
}
const Value::Array& array(const Value& v) {
    auto p=std::get_if<Value::Array>(&v.data);
    if (!p) fail(ErrorCode::schema);
    return *p;
}
const std::string& string(const Value& v) {
    auto p=std::get_if<std::string>(&v.data);
    if (!p) fail(ErrorCode::schema);
    return *p;
}
double number(const Value& v) {
    auto p=std::get_if<double>(&v.data);
    if (!p) fail(ErrorCode::schema);
    return *p;
}
bool boolean(const Value& v) {
    auto p=std::get_if<bool>(&v.data);
    if (!p) fail(ErrorCode::schema);
    return *p;
}
void keys(const Object& o, std::initializer_list<std::string_view> required,
          std::initializer_list<std::string_view> optional={}) {
    for (auto k:required) if (!o.contains(k)) fail(ErrorCode::schema);
    for (const auto& [k,v]:o) {
        (void)v;
        if (std::find(required.begin(),required.end(),k)==required.end() &&
            std::find(optional.begin(),optional.end(),k)==optional.end()) fail(ErrorCode::schema);
    }
}
void one_of(const Value& v, std::initializer_list<std::string_view> choices) {
    const auto& s=string(v);
    if (std::find(choices.begin(),choices.end(),s)==choices.end()) fail(ErrorCode::schema);
}
void equal(const Value& v, std::string_view expected) {
    if (string(v)!=expected) fail(ErrorCode::schema);
}
void integer(const Value& v, double low, double high) {
    const auto n=number(v);
    if (std::trunc(n)!=n || n<low || n>high) fail(ErrorCode::schema);
}
void ratio(const Value& v) {
    auto n=number(v);
    if (n<0 || n>1) fail(ErrorCode::schema);
}
void bounded_id(const Value& v) {
    const auto n=unicode_length(string(v));
    if (!n || n>128) fail(ErrorCode::schema);
}
void f32(const Value& v) {
    const auto n=number(v);
    if (std::abs(n)>static_cast<double>(std::numeric_limits<float>::max()) ||
        static_cast<double>(static_cast<float>(n))!=n) fail(ErrorCode::float32);
}
void u64(const Value& v, bool nonzero=false) { parse_uint64(string(v),nonzero); }
void timestamp(const Value& v) {
    const auto& s=string(v);
    static const std::regex pattern(
        R"(^[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}(\.[0-9]{1,6})?(Z|[+-][0-9]{2}:[0-9]{2})$)");
    if (!std::regex_match(s,pattern)) fail(ErrorCode::schema);
    auto digits=[&](std::size_t pos,std::size_t n) {
        int result=0;
        while (n--) result=result*10+s[pos++]-'0';
        return result;
    };
    const int year=digits(0,4),month=digits(5,2),day=digits(8,2);
    if (year<1 || month<1 || month>12) fail(ErrorCode::timestamp);
    const bool leap=(year%4==0 && (year%100!=0 || year%400==0));
    const int days[]={31,28+(leap?1:0),31,30,31,30,31,31,30,31,30,31};
    if (day<1 || day>days[month-1] || digits(11,2)>23 || digits(14,2)>59 || digits(17,2)>59)
        fail(ErrorCode::timestamp);
    if (s.back()!='Z') {
        const auto offset=s.size()-6;
        if (digits(offset+1,2)>23 || digits(offset+4,2)>59) fail(ErrorCode::timestamp);
    }
}
void provenance(const Value& v) {
    const auto& o=object(v);
    keys(o,{"kind"},{"source_id","timestamp_ms"});
    one_of(o.at("kind"),{"SOURCE_LITERAL","TOOL_EVIDENCE","MODEL_GENERATED","DERIVED_CONTEXT"});
    if (o.contains("source_id") && !std::holds_alternative<std::nullptr_t>(o.at("source_id").data))
        string(o.at("source_id"));
    if (o.contains("timestamp_ms")) u64(o.at("timestamp_ms"));
}
void observation(const Value& v) {
    const auto& o=object(v);
    keys(o,{"resource","metric","value","unit","device_id","observed_at","source_id","confidence","attributes"});
    one_of(o.at("resource"),{"cpu","ram","gpu","npu","battery","thermal","power","system"});
    one_of(o.at("unit"),{"ratio","celsius","watts","count","boolean"});
    static const std::map<std::string,std::string,std::less<>> units={
        {"utilization_ratio","ratio"},{"memory_used_ratio","ratio"},{"temperature_c","celsius"},
        {"power_w","watts"},{"charge_ratio","ratio"},{"charge_rate_w","watts"},
        {"external_power_connected","boolean"},{"queue_depth","count"},
        {"active_work","count"},{"available","boolean"}};
    const auto metric=units.find(string(o.at("metric")));
    if (metric==units.end()) fail(ErrorCode::schema);
    const auto& unit=string(o.at("unit"));
    bounded_id(o.at("device_id")); bounded_id(o.at("source_id"));
    timestamp(o.at("observed_at")); ratio(o.at("confidence"));
    if (!object(o.at("attributes")).empty()) fail(ErrorCode::schema);
    const auto n=number(o.at("value"));
    if (metric->second!=unit) fail(ErrorCode::metric_unit);
    if ((unit=="ratio" && (n<0 || n>1)) || (unit=="count" && n<0) ||
        (unit=="boolean" && n!=0 && n!=1)) fail(ErrorCode::metric_value);
}
void heartbeat(const Value& v) {
    const auto& o=object(v);
    keys(o,{"schema_version","instance_id","instance_type","node_id","sequence","observed_at",
            "state","observations","signals","confidence"});
    equal(o.at("schema_version"),"1.0"); equal(o.at("instance_type"),"heartbeat");
    string(o.at("instance_id")); string(o.at("node_id")); u64(o.at("sequence"));
    one_of(o.at("state"),{"healthy","constrained","degraded","critical","unknown"});
    timestamp(o.at("observed_at")); ratio(o.at("confidence"));
    for (const auto& x:array(o.at("signals"))) string(x);
    for (const auto& x:array(o.at("observations"))) observation(x);
}
void options(const Value& v) {
    const auto& o=object(v);
    keys(o,{"top_p","top_k","repeat_penalty","repeat_last_n","seed","presence_penalty",
            "frequency_penalty","stop_sequences","extra_options"});
    for (auto name:{"top_p","repeat_penalty","presence_penalty","frequency_penalty"}) f32(o.at(name));
    for (auto name:{"top_k","repeat_last_n"}) integer(o.at(name),-2147483648.0,2147483647.0);
    u64(o.at("seed"));
    for (const auto& x:array(o.at("stop_sequences"))) string(x);
    std::string last; bool first=true;
    for (const auto& x:array(o.at("extra_options"))) {
        const auto& pair=array(x);
        if (pair.size()!=2) fail(ErrorCode::schema);
        const auto& key=string(pair[0]); string(pair[1]);
        auto less=[](const std::string& a,const std::string& b) {
            return std::lexicographical_compare(a.begin(),a.end(),b.begin(),b.end(),
                [](unsigned char x,unsigned char y){return x<y;});
        };
        if (!first && !less(last,key)) fail(ErrorCode::option_keys);
        first=false; last=key;
    }
}
void request(const Value& v) {
    const auto& o=object(v);
    keys(o,{"protocol_version","stream","profile","model_id","payload","max_tokens",
            "temperature","stream_requested","has_options"},{"options"});
    equal(o.at("protocol_version"),"0.2");
    one_of(o.at("profile"),{"generate","chat","embed"});
    if (string(o.at("model_id")).empty()) fail(ErrorCode::schema);
    string(o.at("payload")); boolean(o.at("stream_requested"));
    integer(o.at("max_tokens"),0,4294967295.0);
    const auto& stream=object(o.at("stream"));
    keys(stream,{"request_id","execution_id","output_id"});
    u64(stream.at("request_id"),true); u64(stream.at("execution_id"),true);
    integer(stream.at("output_id"),0,4294967295.0);
    f32(o.at("temperature"));
    const bool has=boolean(o.at("has_options"));
    if (has!=o.contains("options")) fail(ErrorCode::schema);
    if (has) options(o.at("options"));
}
constexpr std::array<std::string_view,1> versions={"0.1"};
}
void validate(const Value& root) {
    const auto& o=object(root);
    keys(o,{"type_id","schema_version","provenance","payload"});
    const auto& type=string(o.at("type_id"));
    if (type!="vinox.provenance.snapshot" && type!="liara.heartbeat.snapshot" &&
        type!="linep.v02.request") fail(ErrorCode::type);
    equal(o.at("schema_version"),"0.1");
    provenance(o.at("provenance"));
    if (type=="vinox.provenance.snapshot") {
        if (!object(o.at("payload")).empty()) fail(ErrorCode::schema);
    } else if (type=="liara.heartbeat.snapshot") heartbeat(o.at("payload"));
    else request(o.at("payload"));
}
Envelope decode(std::string_view bytes, std::string_view expected, const Limits& limits) {
    return Envelope::decode(JsonCodec{},bytes,expected,versions,validate,limits);
}
Envelope create(const Value& value, std::string_view expected, const Limits& limits) {
    return Envelope::create(value,expected,versions,validate,limits);
}
}
