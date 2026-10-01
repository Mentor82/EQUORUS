#include "equorus/pilot.hpp"
#include <array>
#include <iostream>
#include <limits>
#include <functional>

using namespace equorus;
void require(bool ok) { if (!ok) throw std::runtime_error("assertion failed"); }
void rejects(ErrorCode code,const std::function<void()>& f) {
    try { f(); } catch (const Error& e) { require(e.code()==code); return; }
    throw std::runtime_error("expected rejection");
}
int main() {
    try {
        JsonCodec codec;
        Value built(Value::Object{
            {"type_id",Value("vinox.provenance.snapshot")},
            {"schema_version",Value("0.1")},
            {"provenance",Value(Value::Object{{"kind",Value("SOURCE_LITERAL")}})},
            {"payload",Value(Value::Object{})}
        });
        auto built_snapshot=pilot::create(built,"vinox.provenance.snapshot");
        require(codec.decode(built_snapshot.encode(codec))==built);
        std::string input=R"({"type_id":"vinox.provenance.snapshot","schema_version":"0.1","provenance":{"kind":"SOURCE_LITERAL","source_id":null,"timestamp_ms":"18446744073709551615"},"payload":{}})";
        const auto original=codec.decode(input);
        auto mutable_value=original;
        auto env=pilot::create(mutable_value,"vinox.provenance.snapshot");
        const auto copy=env;
        input.assign(input.size(),'x');
        std::get<Value::Object>(mutable_value.data).clear();
        require(env.value()==original && copy.value()==original);
        auto encoded=env.encode(codec);
        require(codec.decode(encoded)==original);
        require(parse_uint64("18446744073709551615")==std::numeric_limits<std::uint64_t>::max());
        require(parse_uint64("9007199254740993")==9007199254740993ULL);
        rejects(ErrorCode::uint64_range,[]{parse_uint64("18446744073709551616");});
        rejects(ErrorCode::schema,[]{parse_uint64("1\n");});
        rejects(ErrorCode::type,[&]{pilot::create(original,"linep.v02.request");});
        const std::array<std::string_view,1> wrong_versions={"0.2"};
        rejects(ErrorCode::version,[&]{
            Envelope::create(original,"vinox.provenance.snapshot",wrong_versions,pilot::validate);
        });
        auto invalid=original;
        std::get<Value::Object>(invalid.data)["integrity"]=Value("fake");
        rejects(ErrorCode::schema,[&]{pilot::create(invalid,"vinox.provenance.snapshot");});
        rejects(ErrorCode::number,[&]{codec.encode(Value(-0.0));});
        rejects(ErrorCode::number,[&]{codec.encode(Value(std::numeric_limits<double>::infinity()));});
        rejects(ErrorCode::unicode,[&]{codec.encode(Value(std::string("\xFF")));});
        rejects(ErrorCode::limit,[&]{codec.encode(original,Limits{1,12,2048,8192});});
        rejects(ErrorCode::limit,[&]{codec.decode("[]",Limits{65536,129,2048,8192});});
        rejects(ErrorCode::limit,[&]{codec.decode(std::string(129,'['),Limits{65536,12,2048,8192});});
        rejects(ErrorCode::limit,[&]{codec.decode(R"(["123456789)",Limits{65536,12,2048,2});});
        rejects(ErrorCode::limit,[&]{codec.decode("[0,0,0,",Limits{65536,12,3,8192});});
        rejects(ErrorCode::duplicate_key,[&]{codec.decode(R"({"a":0,"\u0061":1})");});
        rejects(ErrorCode::number,[&]{codec.decode("1e-400");});
        rejects(ErrorCode::number,[&]{codec.decode("-0e999");});
        for (double value:{0.0,1e-300,std::numeric_limits<double>::denorm_min(),9007199254740991.0}) {
            require(codec.decode(codec.encode(Value(value)))==Value(value));
        }
        require(codec.decode(codec.encode(Value(std::string("a\0b",3))))==Value(std::string("a\0b",3)));
        require(codec.decode(R"("\ud83c\udf19")")==Value(std::string("\xF0\x9F\x8C\x99")));
        std::cout<<"core tests passed\n";
        return 0;
    } catch (const Error& e) {
        std::cerr<<e.what()<<"\n"; return 1;
    } catch (const std::exception& e) { std::cerr<<e.what()<<"\n"; return 1; }
}
