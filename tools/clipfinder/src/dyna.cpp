#include "dyna.h"

#include <stdexcept>

////////////////////////////////////////
// JSON (just enough for the viewer's dynapoly export)
////////////////////////////////////////

namespace {

struct JVal {
	enum Kind { Null, Bool, Num, Str, Arr, Obj } kind = Null;
	bool b = false;
	double n = 0;
	string s;
	vector<JVal> a;
	vector<std::pair<string, JVal>> o;
	const JVal* get(const string& k) const {
		for (const auto& kv : o) if (kv.first == k) return &kv.second;
		return nullptr;
	}
};

struct JParser {
	const string& t;
	size_t i = 0;
	explicit JParser(const string& text) : t(text) {}
	[[noreturn]] void fail(const char* what) { throw std::runtime_error(string(what) + " at offset " + std::to_string(i)); }
	void ws() { while (i < t.size() && isspace((unsigned char)t[i])) i++; }
	bool lit(const char* w) {
		size_t n = strlen(w);
		if (t.compare(i, n, w) != 0) return false;
		i += n;
		return true;
	}
	string str() {
		if (t[i] != '"') fail("expected a string");
		i++;
		string o;
		while (i < t.size() && t[i] != '"') {
			char c = t[i++];
			if (c == '\\' && i < t.size()) {
				char e = t[i++];
				switch (e) {
				case 'n': o += '\n'; break;
				case 't': o += '\t'; break;
				case 'r': o += '\r'; break;
				case 'b': o += '\b'; break;
				case 'f': o += '\f'; break;
				case 'u': o += '?'; i += 4; break; // (names are ASCII)
				default: o += e;
				}
			} else o += c;
		}
		if (i >= t.size()) fail("unterminated string");
		i++;
		return o;
	}
	JVal val() {
		ws();
		if (i >= t.size()) fail("unexpected end");
		JVal v;
		char c = t[i];
		if (c == '{') {
			v.kind = JVal::Obj;
			i++;
			ws();
			if (t[i] == '}') { i++; return v; }
			for (;;) {
				ws();
				string k = str();
				ws();
				if (t[i] != ':') fail("expected ':'");
				i++;
				v.o.emplace_back(k, val());
				ws();
				if (t[i] == ',') { i++; continue; }
				if (t[i] == '}') { i++; return v; }
				fail("expected ',' or '}'");
			}
		}
		if (c == '[') {
			v.kind = JVal::Arr;
			i++;
			ws();
			if (t[i] == ']') { i++; return v; }
			for (;;) {
				v.a.push_back(val());
				ws();
				if (t[i] == ',') { i++; continue; }
				if (t[i] == ']') { i++; return v; }
				fail("expected ',' or ']'");
			}
		}
		if (c == '"') { v.kind = JVal::Str; v.s = str(); return v; }
		if (lit("true")) { v.kind = JVal::Bool; v.b = true; return v; }
		if (lit("false")) { v.kind = JVal::Bool; return v; }
		if (lit("null")) return v;
		char* end = nullptr;
		v.n = strtod(t.c_str() + i, &end);
		if (end == t.c_str() + i) fail("bad value");
		v.kind = JVal::Num;
		i = end - t.c_str();
		return v;
	}
};

double num(const JVal* v, const char* what) {
	if (!v || v->kind != JVal::Num) throw std::runtime_error(string("missing number: ") + what);
	return v->n;
}

const JVal& arr(const JVal* v, size_t n, const char* what) {
	if (!v || v->kind != JVal::Arr || (n && v->a.size() != n)) throw std::runtime_error(string("bad array: ") + what);
	return *v;
}

}  // namespace

////////////////////////////////////////
// Dynapoly export
////////////////////////////////////////

// Format (js/wall_push_clips.js exportDynapolys):
// { "format": "dynapoly-1", "game", "map", "numPolygons",
//   "actors": [ { "actor", "sphere": { "center": [x,y,z], "radius" }, "minY", "maxY",
//                 "polys": [ { "v": [[x,y,z] x3], "n": [sx,sy,sz], "d", "type": "wall"|"floor"|"ceiling" } ] } ] }
// Polys are the actor's tangible ones in poly index order, in world space as
// DynaPoly_ExpandSRT leaves them; actors in the order they register (bgId).
bool readDynaFile(const string& path, DynaFile& out, string& err) {
	std::ifstream f(path, std::ios::binary);
	if (!f) { err = "can't read " + path; return false; }
	std::stringstream ss;
	ss << f.rdbuf();
	out.raw = ss.str();
	try {
		JParser p(out.raw);
		JVal root = p.val();
		const JVal* fmt = root.get("format");
		if (!fmt || fmt->kind != JVal::Str || fmt->s != "dynapoly-1") { err = path + " isn't a dynapoly export (format dynapoly-1)"; return false; }
		if (const JVal* g = root.get("game")) out.game = g->s;
		if (const JVal* m = root.get("map")) out.map = m->s;
		if (const JVal* n = root.get("numPolygons")) out.numPolygons = (int)n->n;
		for (const JVal& a : arr(root.get("actors"), 0, "actors").a) {
			DynaActorIn d;
			if (const JVal* nm = a.get("actor")) d.name = nm->s;
			const JVal* sph = a.get("sphere");
			if (!sph) throw std::runtime_error("actor without a sphere");
			const JVal& c = arr(sph->get("center"), 3, "sphere.center");
			for (int k = 0; k < 3; k++) d.center[k] = c.a[k].n;
			d.radius = num(sph->get("radius"), "sphere.radius");
			d.minY = num(a.get("minY"), "minY");
			d.maxY = num(a.get("maxY"), "maxY");
			for (const JVal& pj : arr(a.get("polys"), 0, "polys").a) {
				Model::DynaPolyIn q;
				const JVal& v = arr(pj.get("v"), 3, "poly v");
				for (int k = 0; k < 3; k++) {
					const JVal& vk = arr(&v.a[k], 3, "poly vertex");
					for (int j = 0; j < 3; j++) q.v[k][j] = (int)vk.a[j].n;
				}
				const JVal& n = arr(pj.get("n"), 3, "poly n");
				for (int k = 0; k < 3; k++) q.n[k] = (int)n.a[k].n;
				q.d = (int)num(pj.get("d"), "poly d");
				const JVal* ty = pj.get("type");
				q.type = ty && ty->kind == JVal::Str && !ty->s.empty() ? ty->s[0] : 'w';
				d.polys.push_back(q);
			}
			out.actors.push_back(std::move(d));
		}
	} catch (const std::exception& ex) {
		err = path + ": " + ex.what();
		return false;
	}
	return true;
}

void addDynaActors(Model& m, const DynaFile& d) {
	for (const DynaActorIn& a : d.actors) m.addBgActor(a.name, a.polys, a.center, a.radius, a.minY, a.maxY);
}
