#include "equorus/codec.hpp"
#include <nlohmann/json.hpp>
#include <charconv>
#include <cmath>

namespace equorus {
namespace {
// Allocation-free grammar/budget preflight. nlohmann SAX remains the JSON
// foundation; this pass bounds its token buffers before it sees the input.
class Guard {
    std::string_view s;
    const Limits& l;
    std::size_t p = 0, items = 0;
    void ws() { while (p < s.size() && (s[p]==' ' || s[p]=='\n' || s[p]=='\r' || s[p]=='\t')) ++p; }
    char take() { if (p == s.size()) fail(ErrorCode::malformed); return s[p++]; }
    bool eat(char c) { ws(); if (p < s.size() && s[p] == c) { ++p; return true; } return false; }
    unsigned hex4() {
        unsigned v = 0;
        for (int i=0;i<4;++i) {
            const char c = take();
            v <<= 4;
            if (c>='0' && c<='9') v |= c-'0';
            else if (c>='a' && c<='f') v |= c-'a'+10;
            else if (c>='A' && c<='F') v |= c-'A'+10;
            else fail(ErrorCode::malformed);
        }
        return v;
    }
    void string() {
        if (take() != '"') fail(ErrorCode::malformed);
        std::size_t size = 0;
        for (;;) {
            const auto c = static_cast<unsigned char>(take());
            if (c == '"') return;
            if (c < 0x20) fail(ErrorCode::malformed);
            std::size_t add = 1;
            if (c == '\\') {
                const char e = take();
                if (e == 'u') {
                    auto cp = hex4();
                    if (cp >= 0xD800 && cp <= 0xDBFF) {
                        if (take() != '\\' || take() != 'u') fail(ErrorCode::unicode);
                        const auto low = hex4();
                        if (low < 0xDC00 || low > 0xDFFF) fail(ErrorCode::unicode);
                        cp = 0x10000 + ((cp-0xD800)<<10) + low-0xDC00;
                    } else if (cp >= 0xDC00 && cp <= 0xDFFF) fail(ErrorCode::unicode);
                    add = cp < 0x80 ? 1 : cp < 0x800 ? 2 : cp < 0x10000 ? 3 : 4;
                } else if (std::string_view("\"\\/bfnrt").find(e) == std::string_view::npos)
                    fail(ErrorCode::malformed);
            }
            if (add > l.max_string_length - size) fail(ErrorCode::limit);
            size += add;
        }
    }
    void number() {
        const auto start = p;
        auto digit = [&] { return p < s.size() && s[p]>='0' && s[p]<='9'; };
        if (s[p]=='-') ++p;
        if (!digit()) fail(ErrorCode::malformed);
        if (s[p]=='0') ++p;
        else while (digit()) ++p;
        if (p<s.size() && s[p]=='.') {
            ++p;
            if (!digit()) fail(ErrorCode::malformed);
            while (digit()) ++p;
        }
        if (p<s.size() && (s[p]=='e'||s[p]=='E')) {
            ++p;
            if (p<s.size() && (s[p]=='+'||s[p]=='-')) ++p;
            if (!digit()) fail(ErrorCode::malformed);
            while (digit()) ++p;
        }
        double v{};
        auto [end, ec] = std::from_chars(s.data()+start, s.data()+p, v, std::chars_format::general);
        if (ec != std::errc{} || end != s.data()+p) fail(ErrorCode::number);
        validate_number(v);
    }
    void value(std::size_t depth) {
        ws();
        if (p==s.size()) fail(ErrorCode::malformed);
        if (depth>l.max_depth || items>=l.max_items) fail(ErrorCode::limit);
        ++items;
        char c=s[p];
        if (c=='{' || c=='[') {
            ++p;
            const bool object=c=='{';
            const char close=object?'}':']';
            if (eat(close)) return;
            for (;;) {
                ws();
                if (object) { string(); if (!eat(':')) fail(ErrorCode::malformed); }
                value(depth+1);
                if (eat(close)) return;
                if (!eat(',')) fail(ErrorCode::malformed);
            }
        }
        if (c=='"') { string(); return; }
        if (s.substr(p,3)=="NaN" || s.substr(p,8)=="Infinity" || s.substr(p,9)=="-Infinity")
            fail(ErrorCode::number);
        for (auto literal : {"true","false","null"}) {
            const std::string_view t=literal;
            if (s.substr(p,t.size())==t) { p+=t.size(); return; }
        }
        if (c=='-' || (c>='0'&&c<='9')) { number(); return; }
        fail(ErrorCode::malformed);
    }
public:
    Guard(std::string_view input, const Limits& limits) : s(input), l(limits) {}
    void run() { value(1); ws(); if (p!=s.size()) fail(ErrorCode::malformed); }
};

struct Sax final : nlohmann::json_sax<nlohmann::json> {
    struct Frame { Value value; std::string key; };
    std::vector<Frame> stack;
    Value root;
    bool add(Value v) {
        if (stack.empty()) root=std::move(v);
        else if (auto a=std::get_if<Value::Array>(&stack.back().value.data)) a->push_back(std::move(v));
        else std::get<Value::Object>(stack.back().value.data).emplace(std::move(stack.back().key),std::move(v));
        return true;
    }
    bool null() override { return add(Value(nullptr)); }
    bool boolean(bool v) override { return add(Value(v)); }
    bool number_integer(number_integer_t v) override { return add(Value(static_cast<double>(v))); }
    bool number_unsigned(number_unsigned_t v) override { return add(Value(static_cast<double>(v))); }
    bool number_float(number_float_t v, const string_t&) override { return add(Value(v)); }
    bool string(string_t& v) override { return add(Value(std::move(v))); }
    bool binary(binary_t&) override { fail(ErrorCode::malformed); }
    bool start_object(std::size_t) override { stack.push_back({Value(Value::Object{}),{}}); return true; }
    bool start_array(std::size_t) override { stack.push_back({Value(Value::Array{}),{}}); return true; }
    bool key(string_t& k) override {
        if (std::get<Value::Object>(stack.back().value.data).contains(k)) fail(ErrorCode::duplicate_key);
        stack.back().key=std::move(k);
        return true;
    }
    bool finish() { Value v=std::move(stack.back().value); stack.pop_back(); return add(std::move(v)); }
    bool end_object() override { return finish(); }
    bool end_array() override { return finish(); }
    bool parse_error(std::size_t, const std::string&, const nlohmann::detail::exception&) override {
        fail(ErrorCode::malformed);
    }
};

class Writer {
    const Limits& limits;
    std::string out;
    void append(std::string_view s) {
        if (s.size() > limits.max_bytes - out.size()) fail(ErrorCode::limit);
        out.append(s);
    }
    void string(std::string_view s) {
        static constexpr char hex[]="0123456789abcdef";
        append("\"");
        for (const unsigned char c:s) {
            if (c=='"' || c=='\\') { char b[2]={'\\',static_cast<char>(c)}; append({b,2}); }
            else if (c<0x20) { char b[6]={'\\','u','0','0',hex[c>>4],hex[c&15]}; append({b,6}); }
            else { char b=static_cast<char>(c); append({&b,1}); }
        }
        append("\"");
    }
    void value(const Value& v) {
        if (std::holds_alternative<std::nullptr_t>(v.data)) append("null");
        else if (auto boolean=std::get_if<bool>(&v.data)) append(*boolean?"true":"false");
        else if (auto number=std::get_if<double>(&v.data)) {
            char buffer[64];
            auto [end,ec]=std::to_chars(buffer,buffer+sizeof(buffer),*number,std::chars_format::general);
            if (ec!=std::errc{}) fail(ErrorCode::number);
            append({buffer,static_cast<std::size_t>(end-buffer)});
        } else if (auto text=std::get_if<std::string>(&v.data)) string(*text);
        else if (auto array=std::get_if<Value::Array>(&v.data)) {
            append("["); bool first=true;
            for (const auto& x:*array) { if (!first) append(","); first=false; value(x); }
            append("]");
        } else {
            append("{"); bool first=true;
            for (const auto& [k,x]:std::get<Value::Object>(v.data)) {
                if (!first) append(",");
                first=false; string(k); append(":"); value(x);
            }
            append("}");
        }
    }
public:
    explicit Writer(const Limits& l):limits(l) {}
    std::string run(const Value& v) { value(v); return std::move(out); }
};
}
Value JsonCodec::decode(std::string_view bytes, const Limits& limits) const {
    check_limits(limits);
    if (bytes.size()>limits.max_bytes) fail(ErrorCode::limit);
    try { unicode_length(bytes); } catch (const Error&) { fail(ErrorCode::malformed); }
    Guard(bytes,limits).run();
    Sax sax;
    if (!nlohmann::json::sax_parse(bytes.begin(),bytes.end(),&sax)) fail(ErrorCode::malformed);
    return std::move(sax.root);
}
std::string JsonCodec::encode(const Value& value, const Limits& limits) const {
    validate_value(value,limits);
    return Writer(limits).run(value);
}
} // namespace equorus
