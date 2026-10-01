"""Run the same corpus through independent C++ and Python implementations."""
import copy
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "python"))
from equorus_reference import Envelope, JsonCodec, ContractError, Limits, uint64

FIXTURES = ROOT / "tests/fixtures/pilot-v0.1"

def equivalent(a,b):
    if type(a) in (int,float) and type(b) in (int,float):
        return a == b
    if type(a) is not type(b):
        return False
    if isinstance(a,dict):
        return a.keys() == b.keys() and all(equivalent(a[k],b[k]) for k in a)
    if isinstance(a,list):
        return len(a) == len(b) and all(equivalent(x,y) for x,y in zip(a,b))
    return a == b

def case_bytes(case):
    if "raw" in case:
        return case["raw"].encode("utf-8")
    if "raw_hex" in case:
        return bytes.fromhex(case["raw_hex"])
    raw=(FIXTURES / case["base"]).read_bytes()
    if case.get("changes"):
        value=json.loads(raw)
        for change in case["changes"]:
            parent=value
            for key in change["path"][:-1]:
                parent=parent[key]
            key=change["path"][-1]
            if change["op"]=="remove":
                del parent[key]
            else:
                parent[key]=change["value"]
        raw=json.dumps(value,ensure_ascii=True,allow_nan=False).encode("utf-8")
    return raw

def native(binary, raw, expected, limits):
    args=[binary,expected]+[str(getattr(limits,k)) for k in vars(limits)]
    r=subprocess.run(args,input=raw,capture_output=True,timeout=15)
    if r.returncode==0:
        return "accept",r.stdout
    if r.returncode==1 and r.stdout.startswith(b"ERROR "):
        return r.stdout[6:].decode(),None
    raise AssertionError(f"C++ process failed ({r.returncode}): {r.stderr!r} {r.stdout!r}")

def main():
    binary=sys.argv[1]
    suite=json.loads((FIXTURES/"cases.json").read_text(encoding="utf-8"))
    accepted=0
    for case in suite:
        raw=case_bytes(case)
        expected=case.get("expected_type")
        if expected is None:
            if "base" in case:
                expected=json.loads((FIXTURES/case["base"]).read_bytes())["type_id"]
            else:
                expected="vinox.provenance.snapshot"
        limits=Limits(**case.get("limits",{}))
        actual,out=native(binary,raw,expected,limits)
        assert actual==case["expect"],f"C++ {case['name']}: {actual} != {case['expect']}"
        try:
            env=Envelope.decode(raw,expected,limits=limits)
            pyout=env.encode(limits=limits)
            pyactual="accept"
        except ContractError as error:
            pyactual=str(error)
        assert pyactual==case["expect"],f"Python {case['name']}: {pyactual} != {case['expect']}"
        if actual=="accept":
            accepted+=1
            original=json.loads(raw)
            assert equivalent(original,json.loads(out)),case["name"]
            assert equivalent(original,json.loads(pyout)),case["name"]
            # C++ -> Python -> C++, including uint64 strings and field presence.
            from_cpp=Envelope.decode(out,expected,limits=limits).encode(limits=limits)
            code,back=native(binary,from_cpp,expected,limits)
            assert code=="accept" and equivalent(original,json.loads(back)),case["name"]
            code,back=native(binary,pyout,expected,limits)
            assert code=="accept" and equivalent(original,json.loads(back)),case["name"]

    # Native Python construction and deep snapshot semantics.
    for p in FIXTURES.glob("*.json"):
        if p.name=="cases.json":
            continue
        value=json.loads(p.read_bytes())
        before=copy.deepcopy(value)
        env=Envelope(value,value["type_id"])
        value["provenance"].clear()
        detached=env.value
        detached["payload"].clear()
        assert equivalent(env.value,before)
        code,out=native(binary,env.encode(),before["type_id"],Limits())
        assert code=="accept" and equivalent(json.loads(out),before)
    assert uint64("18446744073709551615")==2**64-1

    # Independently implemented validators must agree on structural type errors.
    mutations=0
    for p in FIXTURES.glob("*.json"):
        if p.name=="cases.json":
            continue
        original=json.loads(p.read_bytes())
        locations=[]
        def collect(value, path=()):
            if isinstance(value,dict):
                for key,child in value.items():
                    locations.append(path+(key,))
                    collect(child,path+(key,))
        collect(original)
        for location in locations:
            for replacement in (None,False,0,0.5,"",[],{},"0.2"):
                value=copy.deepcopy(original)
                parent=value
                for key in location[:-1]:
                    parent=parent[key]
                parent[location[-1]]=replacement
                raw=json.dumps(value).encode()
                code,_=native(binary,raw,original["type_id"],Limits())
                try:
                    Envelope.decode(raw,original["type_id"])
                    pycode="accept"
                except ContractError as error:
                    pycode=str(error)
                assert (code=="accept")== (pycode=="accept"),(p.name,location,code,pycode)
                mutations+=1
    try:
        JsonCodec().encode(10**1000)
    except ContractError as error:
        assert str(error)=="NUMBER"
    else:
        raise AssertionError("oversized native integer accepted")

    # Additional malformed/boundary corpus shared independently of schema fixtures.
    extra=[
        (b'{"a":0,"\\u0061":1}',"DUPLICATE_KEY",Limits()),
        (b'['*129,"LIMIT",Limits()),
        (b'["123456789',"LIMIT",Limits(max_string_length=2)),
        (b'[0,0,0,',"LIMIT",Limits(max_items=3)),
        (b'1e-400',"NUMBER",Limits()),
        (b'-0e999',"NUMBER",Limits()),
        (b'{"x":"\\udc00"}',"UNICODE",Limits()),
        (b'{"x":"\\ud800\\u0041"}',"UNICODE",Limits()),
        (b'{"x":"\xc0\x80"}',"MALFORMED",Limits()),
        (b'{}',"LIMIT",Limits(max_depth=129)),
    ]
    for raw,expect,limits in extra:
        code,_=native(binary,raw,"vinox.provenance.snapshot",limits)
        assert code==expect,(raw,code)
        try:
            Envelope.decode(raw,"vinox.provenance.snapshot",limits=limits)
        except ContractError as error:
            assert str(error)==expect,(raw,str(error))
        else:
            raise AssertionError(raw)
    print(f"PASS: {len(suite)} shared cases; {accepted} bidirectional roundtrips; "
          f"6 native Python snapshots; {len(extra)} adversarial cases; {mutations} type mutations")

if __name__=="__main__":
    main()
